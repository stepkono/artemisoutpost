// Fill out your copyright notice in the Description page of Project Settings.


#include "MapCutoutManager.h"

#include "ArtemisOutpost/Miscellaneous/DataTypes.h"
#include "Kismet/KismetMaterialLibrary.h"


// Sets default values for this component's properties
UMapCutoutManager::UMapCutoutManager()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UMapCutoutManager::BeginPlay()
{
	Super::BeginPlay();
	
	if ((MoonGeoRef = GetOwner()))
	{
		InitialMoonScalingFactor = MoonGeoRef->GetActorScale3D().X; 
	}
	
	if (AGameStateBase* DefaultGS = GetWorld()->GetGameState())
	{
		GS = Cast<AArtemisGameState>(DefaultGS);
		
		if (!GS)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to cast to ArtemisGameState."));
			return; 
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Unable to get GameState.")); 
		return; 
	}
	
	// Spatial anchors require the OculusXR runtime which is not available on dedicated servers
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	
	AnchorsManager = GetWorld()->GetGameInstance()->GetSubsystem<UAnchorsManagerSubsystem>();
	
	GS->OnRawAnchorsUpdated.AddDynamic(this, &UMapCutoutManager::HandleAnchorsUpdate);
	
	if (!IsAuthoritativeClient())
	{
		UE_LOG(LogTemp, Log, TEXT("MapCutoutManager: Not authoritative client."))
		
		FOrderedAnchors AnchorsFromServer; 
		if (GS->GetAnchorsFromPreviousSessions(AnchorsFromServer))
		{
			HandleAnchorsUpdate(AnchorsFromServer); 	
		}	
	}
}


// Called every frame
void UMapCutoutManager::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bAnchorsSpawned)
	{
		return; 
	}
	
	UpdateMaterialParamCollection();
}

void UMapCutoutManager::HandleAnchorsUpdate(const FOrderedAnchors& RawAnchors)
{
	UE_LOG(LogTemp, Log, TEXT("MapCutoutManager: Received new anchors to handle...")); 
	
	if (IsAuthoritativeClient())
	{
		AnchorsManager->DiscoverAnchors(RawAnchors, [this](TArray<AActor*> SpawnedOrderedAnchors)
		{
			HandleAnchorsSpawned();
		}); 
	}
	else
	{
		AnchorsManager->RequestSharedAnchors(RawAnchors, [this](TArray<AActor*> SpawnedOrderedAnchors)
		{
			HandleAnchorsSpawned(); 
		}); 
	}
}

void UMapCutoutManager::HandleAnchorsSpawned()
{
	UE_LOG(LogTemp, Log, TEXT("MapCutoutManager: Handling physical anchor positions...")); 
	
	BindGeoRefToAnchor();
	
	CalibrateAnchors(AnchorsManager->GetAnchors());
	
	bAnchorsSpawned = true; 
}

void UMapCutoutManager::CalibrateAnchors(TArray<AActor*> Anchors)
{
	const FVector A = Anchors[0]->GetActorLocation();
	const FVector B = Anchors[1]->GetActorLocation();
	const FVector C = Anchors[2]->GetActorLocation();
	const FVector D = Anchors[3]->GetActorLocation();

	// Flatten edge vectors onto the horizontal plane
	const FVector AB = FVector(B.X - A.X, B.Y - A.Y, 0.f);
	FVector AD = FVector(D.X - A.X, D.Y - A.Y, 0.f);

	// Gram-Schmidt: force AD perpendicular to AB
	// Projects out the AB component from AD, leaving only the orthogonal part
	const FVector ABNorm = AB.GetSafeNormal();
	AD = AD - FVector::DotProduct(AD, ABNorm) * ABNorm;

	// Reconstruct clean corners: A stays fixed, B/D/C derived from orthogonal edges
	AnchorPositions.AAnchorPos = A;
	AnchorPositions.BAnchorPos = AnchorPositions.AAnchorPos + AB;
	AnchorPositions.DAnchorPos = AnchorPositions.AAnchorPos + AD;
	AnchorPositions.CAnchorPos = AnchorPositions.AAnchorPos + AB + AD;
	
	const FVector TableCenter = (AnchorPositions.AAnchorPos + AnchorPositions.BAnchorPos + AnchorPositions.CAnchorPos + AnchorPositions.DAnchorPos) * 0.25;
	const FVector TableNormal = FVector::CrossProduct(AnchorPositions.BAnchorPos - AnchorPositions.AAnchorPos, AnchorPositions.DAnchorPos - AnchorPositions.AAnchorPos).GetSafeNormal(); 
	
	UE_LOG(LogTemp, Warning, TEXT("CalibrateAnchors: Table center: %s"), *TableCenter.ToString());
	UE_LOG(LogTemp, Warning, TEXT("CalibrateAnchors: Table normal: %s"), *TableNormal.ToString());
	
	PutGeoRefIntoTablePlane(TableCenter, TableNormal);
}

void UMapCutoutManager::BindGeoRefToAnchor() const
{
	AActor* AAnchor = AnchorsManager->GetAnchors()[0];
	
	constexpr EAttachmentRule AttachmentRule = EAttachmentRule::SnapToTarget; 
	const FAttachmentTransformRules AttachmentTransformRule = FAttachmentTransformRules(AttachmentRule, true);

	GetOwner()->AttachToActor(AAnchor, AttachmentTransformRule);
	GetOwner()->SetActorRelativeLocation(FVector::Zero());
	
	UE_LOG(LogTemp, Log, TEXT("Owner Location AFTER transform: %s"), *GetOwner()->GetActorLocation().ToString());
}

void UMapCutoutManager::PutGeoRefIntoTablePlane(const FVector& TableCenter, const FVector& TableNormal) const
{
	const AActor* AAnchor = AnchorsManager->GetAnchors()[0];
	
	const FVector DiagonalOffset = TableCenter - AAnchor->GetActorLocation();
	const FVector VerticalOffset = -1 * TableNormal * (173806785 * InitialMoonScalingFactor); 
	const FVector FinalOffset = DiagonalOffset + VerticalOffset;

	MoonGeoRef->AddActorLocalOffset(FinalOffset);
	
	UE_LOG(LogTemp, Log, TEXT("Moon set to: %s"), *GetOwner()->GetActorLocation().ToString());
	
	OnCutoutSet.Broadcast();
}

void UMapCutoutManager::UpdateMaterialParamCollection() const
{
	const AActor* AAnchor = AnchorsManager->GetAnchors()[0];
	const AActor* BAnchor = AnchorsManager->GetAnchors()[1];
	const AActor* CAnchor = AnchorsManager->GetAnchors()[2];
	const AActor* DAnchor = AnchorsManager->GetAnchors()[3];
	
	const FLinearColor NewAnchorA(AAnchor->GetActorLocation()); 
	const FLinearColor NewAnchorB(BAnchor->GetActorLocation());
	const FLinearColor NewAnchorC(CAnchor->GetActorLocation());
	const FLinearColor NewAnchorD(DAnchor->GetActorLocation());
	
	UKismetMaterialLibrary::SetVectorParameterValue(GetWorld(), AnchorsCollection, FName("AnchorA"), NewAnchorA); 
	UKismetMaterialLibrary::SetVectorParameterValue(GetWorld(), AnchorsCollection, FName("AnchorB"), NewAnchorB); 
	UKismetMaterialLibrary::SetVectorParameterValue(GetWorld(), AnchorsCollection, FName("AnchorC"), NewAnchorC); 
	UKismetMaterialLibrary::SetVectorParameterValue(GetWorld(), AnchorsCollection, FName("AnchorD"), NewAnchorD); 
}

bool UMapCutoutManager::IsAuthoritativeClient() const
{
	UArtemisGameInstance* GI; 
	if (UGameInstance* DefaultGI = GetWorld()->GetGameInstance())
	{
		GI = Cast<UArtemisGameInstance>(DefaultGI);
		if (GI)
		{
			return GI->CheckForInitializedSpatialAnchors(); 
		}
		
		UE_LOG(LogTemp, Error, TEXT("UMapCutoutManager: Failed to cast to custom game state."));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("UMapCutoutManager: Failed to get the default game state."));
	}
	
	return false; 
}