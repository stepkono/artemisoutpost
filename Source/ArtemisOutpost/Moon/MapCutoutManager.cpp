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

	// TODO: not sure if this is even necessary. Because every client gets the replicated vars from  GameState. Question is, if the HandleAnchorsUpdate will be triggered. 
	if (!IsAuthoritativeClient())
	{
		UE_LOG(LogTemp, Log, TEXT("MapCutoutManager: Not authoritative client."))

		FOrderedAnchors AnchorsFromServer;
		if (GS->GetAnchorsFromCurrentSession(AnchorsFromServer))
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
	// B(top-left)     C(top-right)
	// A(bottom-left)  D(bottom-right)
	TArray<AActor*> Anchors = AnchorsManager->GetAnchors();

	// Require all four slots to be valid — a failed/empty retrieval yields 4 null sentinels,
	// which pass a plain Num() check but would crash on GetActorLocation() below.
	if (Anchors.Num() < 4 || !IsValid(Anchors[0]) || !IsValid(Anchors[1])
		|| !IsValid(Anchors[2]) || !IsValid(Anchors[3]))
	{
		UE_LOG(LogTemp, Warning, TEXT("MapCutoutManager: Anchor set is incomplete or invalid. Aborting handling."));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("MapCutoutManager: Handling physical anchor positions..."));

	BindGeoRefToAnchor();
	FCalibratedData CalibratedData = UGeoUtils::CalibrateAnchors(
		Anchors[0]->GetActorLocation(),   // A — origin (bottom-left)
		Anchors[1]->GetActorLocation(),   // B — upward edge (top-left)
		Anchors[3]->GetActorLocation()    // D — rightward edge (bottom-right)
		);

	PutGeoRefIntoTablePlane(CalibratedData.PlaneCenter, CalibratedData.PlaneNormal);
	
	GetWorld()->GetGameInstance()->GetSubsystem<UXRUtilsSubsystem>()->InitXRTRansform();
	
	bAnchorsSpawned = true; 
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
	
	//GetOwner()->GetRootComponent()->SetAbsolute(false,true, true);
	//GetOwner()->GetRootComponent()->AttachToComponent(AAnchor->GetRootComponent(), AttachmentTransformRule);
	//GetOwner()->SetActorRelativeLocation(FVector::Zero());
	
	GetOwner()->SetActorLocation(AAnchor->GetActorLocation());
	
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