// Fill out your copyright notice in the Description page of Project Settings.


#include "MapCutoutManager.h"

#include "ArtemisOutpost/TransformationsManager.h"
#include "ArtemisOutpost/Miscellaneous/DataTypes.h"
#include "ArtemisOutpost/Miscellaneous/GeoUtils.h"
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
			UE_LOG(LogTemp, Error, TEXT("MapCutoutManager: Failed to cast to ArtemisGameState."));
			return;
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("MapCutout: Unable to get GameState."));
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
		AnchorsManager->DiscoverAnchors(RawAnchors, [this](TArray<AActor*> &SpawnedOrderedAnchors)
		{
			if (SpawnedOrderedAnchors.Num() < 4)
			{
				UE_LOG(LogTemp, Warning, TEXT("MapCutoutManager: Aborting anchors handling, because %d anchors were spawned, instead of 4."), SpawnedOrderedAnchors.Num())	
				return; 
			}
			HandleAnchorsSpawned(); 
		}); 
	}
	else
	{
		AnchorsManager->RequestSharedAnchors(RawAnchors, [this](TArray<AActor*> &SpawnedOrderedAnchors)
		{
			if (SpawnedOrderedAnchors.Num() < 4)
			{
				UE_LOG(LogTemp, Warning, TEXT("MapCutoutManager: Aborting anchors handling, because %d anchors were spawned, instead of 4."), SpawnedOrderedAnchors.Num());
				return; 
			}
			HandleAnchorsSpawned(); 
		}); 
	}
}

void UMapCutoutManager::HandleAnchorsSpawned()
{
	if (AnchorsManager->GetAnchors().Num() < 4)
	{
		return; 
	}
	
	UE_LOG(LogTemp, Log, TEXT("MapCutoutManager: Handling physical anchor positions...")); 
	
	BindGeoRefToAnchor();
	
	// B(top-left)     C(top-right)
	// A(bottom-left)  D(bottom-right)
	TArray<AActor*> Anchors = AnchorsManager->GetAnchors();
	FCalibratedData CalibratedData = UGeoUtils::CalibrateAnchors(
		Anchors[0]->GetActorLocation(),   // A — origin (bottom-left)
		Anchors[1]->GetActorLocation(),   // B — upward edge (top-left)
		Anchors[3]->GetActorLocation()    // D — rightward edge (bottom-right)
		);

	PutGeoRefIntoTablePlane(CalibratedData.PlaneCenter, CalibratedData.PlaneNormal);
	
	bAnchorsSpawned = true; 
}

void UMapCutoutManager::CalibrateAnchors()
{
	if (AnchorsManager->GetAnchors().Num() < 4)
	{
		UE_LOG(LogTemp, Warning, TEXT("MapCutoutManager: CalibrateAnchors: Anchors array in AnchorsManager has length: %d. Aborting."), AnchorsManager->GetAnchors().Num());
		return; 
	}
	
	// B(top-left)     C(top-right)
	// A(bottom-left)  D(bottom-right)
	TArray<AActor*> Anchors = AnchorsManager->GetAnchors();

	const FVector A = Anchors[0]->GetActorLocation();  // origin — bottom-left
	const FVector B = Anchors[1]->GetActorLocation();  // top-left
	const FVector D = Anchors[3]->GetActorLocation();  // bottom-right

	// Flatten edge vectors onto the horizontal plane
	const FVector AB = FVector(B.X - A.X, B.Y - A.Y, 0.f); 
	FVector AD       = FVector(D.X - A.X, D.Y - A.Y, 0.f);  

	// Gram-Schmidt: force AD perpendicular to AB
	const FVector ABNorm = AB.GetSafeNormal();
	AD = AD - FVector::DotProduct(AD, ABNorm) * ABNorm;

	// Reconstruct clean corners with A as origin
	AnchorPositions.AAnchorPos = A;           // bottom-left
	AnchorPositions.BAnchorPos = A + AB;      // top-left
	AnchorPositions.CAnchorPos = A + AD;      // bottom-right
	AnchorPositions.DAnchorPos = A + AB + AD; // top-right

	const FVector TableCenter = (AnchorPositions.AAnchorPos + AnchorPositions.BAnchorPos + AnchorPositions.CAnchorPos + AnchorPositions.DAnchorPos) * 0.25;
	const FVector TableNormal = FVector::CrossProduct(AB, AD).GetSafeNormal();
	
	UE_LOG(LogTemp, Warning, TEXT("CalibrateAnchors: Table center: %s"), *TableCenter.ToString());
	UE_LOG(LogTemp, Warning, TEXT("CalibrateAnchors: Table normal: %s"), *TableNormal.ToString());
	
	PutGeoRefIntoTablePlane(TableCenter, TableNormal);
}

void UMapCutoutManager::BindGeoRefToAnchor() const
{
	if (AnchorsManager->GetAnchors().Num() < 4)
	{
		UE_LOG(LogTemp, Warning, TEXT("MapCutoutManager: BindGeoRefToAnchor: Anchors array in AnchorsManager has length: %d. Aborting."), AnchorsManager->GetAnchors().Num());
		return; 
	}
	
	AActor* AAnchor = AnchorsManager->GetAnchors()[0];
	
	const FAttachmentTransformRules AttachmentTransformRule(
		EAttachmentRule::SnapToTarget,
		EAttachmentRule::KeepWorld,     
		EAttachmentRule::KeepWorld,    
		true
	);
	
	GetOwner()->GetRootComponent()->SetAbsolute(false,true, true);
	GetOwner()->GetRootComponent()->AttachToComponent(AAnchor->GetRootComponent(), AttachmentTransformRule);
	GetOwner()->SetActorRelativeLocation(FVector::Zero());
	
	//GetOwner()->SetActorLocation(AAnchor->GetActorLocation());
	
	UE_LOG(LogTemp, Log, TEXT("Moon Location AFTER assigned to Anchor: %s"), *GetOwner()->GetActorLocation().ToString());
}

void UMapCutoutManager::PutGeoRefIntoTablePlane(const FVector& TableCenter, const FVector& TableNormal) const
{
	const AActor* AAnchor = AnchorsManager->GetAnchors()[0];
	
	const FVector DiagonalOffset = TableCenter - AAnchor->GetActorLocation();
	const FVector VerticalOffset = -1 * TableNormal * (173806785 * InitialMoonScalingFactor); 
	const FVector FinalOffset = DiagonalOffset + VerticalOffset;

	MoonGeoRef->AddActorWorldOffset(FinalOffset);
	
	UE_LOG(LogTemp, Log, TEXT("Moon set into table plane: %s"), *GetOwner()->GetActorLocation().ToString());
	
	GetWorld()->GetSubsystem<UTransformationsManager>()->Activate(TableCenter, TableNormal);
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