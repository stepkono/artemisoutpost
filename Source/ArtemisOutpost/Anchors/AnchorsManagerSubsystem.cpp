// Fill out your copyright notice in the Description page of Project Settings.

#include "AnchorsManagerSubsystem.h"

#include "ArtemisAnchorSettings.h"
#include "OculusXRAnchorBPFunctionLibrary.h"
#include "OculusXRAnchors.h"

void UAnchorsManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Bind discovery callbacks once — reused across all DiscoverAnchors calls
	DiscoveredAnchorDelegate.BindUObject(this, &UAnchorsManagerSubsystem::OnAnchorDiscovered);
	DiscoveredAnchorsCompleteDelegate.BindUObject(this, &UAnchorsManagerSubsystem::OnDiscoveryComplete);
}

void UAnchorsManagerSubsystem::DiscoverAnchors(const FCustomAnchors& RawAnchors, TFunction<void(TArray<AActor*>)> OnComplete)
{
	// Store callback — fired when discovery is fully complete
	PendingCallback = OnComplete;
	RawAnchorsToSpawn.Reset();

	// Record A/B/C/D order so we can restore it after unordered discovery results
	OrderedUUIDs = { RawAnchors.AAnchorUUID, RawAnchors.BAnchorUUID, RawAnchors.CAnchorUUID, RawAnchors.DAnchorUUID };

	// Build UUID filter — create filter once, add all UUIDs, then add to DiscoveryInfo
	UOculusXRSpaceDiscoveryIdsFilter* IDsFilter = NewObject<UOculusXRSpaceDiscoveryIdsFilter>();
	IDsFilter->Uuids = OrderedUUIDs;

	FOculusXRSpaceDiscoveryInfo DiscoveryInfo;
	DiscoveryInfo.Filters.Add(IDsFilter);

	EOculusXRAnchorResult::Type OutResult;
	OculusXRAnchors::FOculusXRAnchors::DiscoverAnchors(
		DiscoveryInfo,
		DiscoveredAnchorDelegate,
		DiscoveredAnchorsCompleteDelegate,
		OutResult
	);

	if (OutResult != EOculusXRAnchorResult::Success)
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnchorsManagerSubsystem: DiscoverAnchors failed to start. Result: %d"), (int32)OutResult);
	}
}

void UAnchorsManagerSubsystem::OnAnchorDiscovered(const TArray<FOculusXRAnchorsDiscoverResult>& DiscoveredAnchors)
{
	for (const FOculusXRAnchorsDiscoverResult& Incoming : DiscoveredAnchors)
	{
		const bool bAlreadyExists = RawAnchorsToSpawn.ContainsByPredicate(
			[&Incoming](const FOculusXRAnchorsDiscoverResult& Existing)
			{
				return Existing.UUID == Incoming.UUID;
			}
		);

		if (!bAlreadyExists)
		{
			RawAnchorsToSpawn.Add(Incoming);
		}
	}
}

void UAnchorsManagerSubsystem::OnDiscoveryComplete(EOculusXRAnchorResult::Type Result)
{
	if (Result != EOculusXRAnchorResult::Success)
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnchorsManagerSubsystem: Discovery completed with error. Result: %d"), (int32)Result);
	}
	
	// Load the Blueprint anchor actor class from Project Settings
	const UArtemisAnchorSettings* Settings = GetDefault<UArtemisAnchorSettings>();
	UClass* AnchorClass = Settings->SpatialAnchorModelClass.LoadSynchronous();
	if (!AnchorClass)
	{
		UE_LOG(LogTemp, Error, TEXT("UAnchorsManagerSubsystem: SpatialAnchorModelClass is not set."));
		return;
	}
	
	// Spawn in A/B/C/D order by matching each ordered UUID to its discovery result
	TArray<AActor*> SpawnedAnchors;
	for (const FOculusXRUUID& UUID : OrderedUUIDs)
	{
		const FOculusXRAnchorsDiscoverResult* Found = RawAnchorsToSpawn.FindByPredicate(
			[&UUID](const FOculusXRAnchorsDiscoverResult& R) { return R.UUID == UUID; }
		);

		if (!Found)
		{
			UE_LOG(LogTemp, Warning, TEXT("UAnchorsManagerSubsystem: No discovery result for one of the expected UUIDs."));
			SpawnedAnchors.Add(nullptr); // preserve index alignment
			continue;
		}

		AActor* SpawnedAnchor = UOculusXRAnchorBPFunctionLibrary::SpawnActorWithAnchorHandle(
			GetWorld(),
			Found->Space,
			Found->UUID,
			EOculusXRSpaceStorageLocation::Local,
			AnchorClass,
			nullptr,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn
		);

		if (SpawnedAnchor)
		{
			SpawnedAnchor->SetActorHiddenInGame(true);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("UAnchorsManagerSubsystem: Failed to spawn anchor actor for UUID."));
		}
		
		SpawnedAnchors.Add(SpawnedAnchor); // nullptr slots preserved for index alignment
	}
	
	if (SpawnedAnchors.Num() > 0)
	{
		BaseAnchor = SpawnedAnchors[0]; 	
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("No Anchors were spawned."));
	}
	
	// Fire callback with actors in A(0) B(1) C(2) D(3) order
	if (PendingCallback)
	{
		PendingCallback(SpawnedAnchors);
		PendingCallback = nullptr;
	}

	RawAnchorsToSpawn.Reset();
}

AActor* UAnchorsManagerSubsystem::GetBaseAnchor()
{
	return BaseAnchor;
}