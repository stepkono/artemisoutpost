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
	
	if (AActor* Moon = GetOwner())
	{
		InitialMoonScalingFactor = Moon->GetActorScale3D().X; 
	}
	
	if (AGameStateBase* DefaultGI = GetWorld()->GetGameState())
	{
		GS = Cast<AArtemisGameState>(DefaultGI);
		
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

	// ...
}

void UMapCutoutManager::HandleAnchorsUpdate(const FOrderedAnchors& RawAnchors)
{
	UE_LOG(LogTemp, Log, TEXT("MapCutoutManager: Received new anchors to handle...")); 
	
	if (IsAuthoritativeClient())
	{
		AnchorsManager->DiscoverAnchors(RawAnchors, [this](TArray<AActor*> SpawnedOrderedAnchors)
		{
			HandleAnchorsSpawned(SpawnedOrderedAnchors);
		}); 
	}
	else
	{
		AnchorsManager->RequestSharedAnchors(RawAnchors, [this](TArray<AActor*> SpawnedOrderedAnchors)
		{
			HandleAnchorsSpawned(SpawnedOrderedAnchors); 
		}); 
	}
}

void UMapCutoutManager::HandleAnchorsSpawned(TArray<AActor*>& SpawnedOrderedAnchors)
{
	UE_LOG(LogTemp, Log, TEXT("MapCutoutManager: Handling physical anchor positions...")); 
	
	if (SpawnedOrderedAnchors.Num() != 4)
	{
		UE_LOG(LogTemp, Warning, TEXT("MapCutoutManager: SpawnedOrderedAnchors array is misformed.")); 
		return; 
	}
	
	FOrderedAnchorsPositions RawAnchorsPositions; 
	
	if (SpawnedOrderedAnchors[0])
	{
		RawAnchorsPositions.AAnchorPos = SpawnedOrderedAnchors[0]->GetActorLocation();
		BindGeoRefToAnchor(SpawnedOrderedAnchors[0]);
	}
	if (SpawnedOrderedAnchors[1])
	{
		RawAnchorsPositions.AAnchorPos = SpawnedOrderedAnchors[1]->GetActorLocation();
	}
	if (SpawnedOrderedAnchors[2])
	{
		RawAnchorsPositions.BAnchorPos = SpawnedOrderedAnchors[2]->GetActorLocation();	
	}
	if (SpawnedOrderedAnchors[3])
	{
		RawAnchorsPositions.DAnchorPos = SpawnedOrderedAnchors[3]->GetActorLocation();	
	}
	
	CalibrateAnchors(RawAnchorsPositions);
}

void UMapCutoutManager::CalibrateAnchors(FOrderedAnchorsPositions& RawAnchorsPositions)
{
	const FVector A = RawAnchorsPositions.AAnchorPos;
	const FVector B = RawAnchorsPositions.BAnchorPos;
	const FVector C = RawAnchorsPositions.CAnchorPos;
	const FVector D = RawAnchorsPositions.DAnchorPos;

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
	const FVector TableNormal = FVector::CrossProduct((AnchorPositions.DAnchorPos - AnchorPositions.AAnchorPos),(AnchorPositions.BAnchorPos - AnchorPositions.AAnchorPos)); 
	
	PutGeoRefIntoTablePlane(TableCenter, TableNormal);
	
	UpdateMaterialParamCollection();
}

void UMapCutoutManager::UpdateMaterialParamCollection() const
{
	const FLinearColor NewAnchorA(AnchorPositions.AAnchorPos); 
	const FLinearColor NewAnchorB(AnchorPositions.BAnchorPos);
	const FLinearColor NewAnchorC(AnchorPositions.CAnchorPos);
	const FLinearColor NewAnchorD(AnchorPositions.DAnchorPos);
	
	UKismetMaterialLibrary::SetVectorParameterValue(GetWorld(), AnchorsCollection, FName("V1"), NewAnchorA); 
	UKismetMaterialLibrary::SetVectorParameterValue(GetWorld(), AnchorsCollection, FName("V2"), NewAnchorB); 
	UKismetMaterialLibrary::SetVectorParameterValue(GetWorld(), AnchorsCollection, FName("V3"), NewAnchorC); 
	UKismetMaterialLibrary::SetVectorParameterValue(GetWorld(), AnchorsCollection, FName("V4"), NewAnchorD); 
}

void UMapCutoutManager::BindGeoRefToAnchor(AActor* SpawnedAnchor) const
{
	constexpr EAttachmentRule AttachmentRule = EAttachmentRule::KeepRelative; 
	const FAttachmentTransformRules AttachmentTransformRule = FAttachmentTransformRules(AttachmentRule, true); 
	GetOwner()->AttachToActor(SpawnedAnchor, AttachmentTransformRule);
	GetOwner()->SetActorRelativeLocation(FVector::Zero());
}

void UMapCutoutManager::PutGeoRefIntoTablePlane(const FVector& TableCenter, const FVector& TableNormal) const
{
	const FVector DiagonalOffset = TableCenter - AnchorPositions.AAnchorPos;
	const FVector VerticalOffset = TableNormal * (173806785 * InitialMoonScalingFactor); 
	const FVector FinalOffset = AnchorPositions.AAnchorPos + DiagonalOffset + VerticalOffset;
	
	GetOwner()->AddActorLocalOffset(FinalOffset);
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