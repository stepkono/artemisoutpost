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

	// Load once at startup — cheap here, instant everywhere else
	const UArtemisAnchorSettings* Settings = GetDefault<UArtemisAnchorSettings>();
	AnchorClass = Settings->SpatialAnchorModelClass.LoadSynchronous();

	if (!AnchorClass)
	{
		UE_LOG(LogTemp, Error, TEXT("UAnchorsManagerSubsystem: SpatialAnchorModelClass is not set in Project Settings."));
	}
}

void UAnchorsManagerSubsystem::DiscoverAnchors(const FCustomAnchors& RawAnchors, TFunction<void(TArray<AActor*>)> OnComplete)
{
	// Store callback — fired when discovery is fully complete
	PendingCallback = OnComplete;
	UnorderedDiscoveredAnchors.Reset();

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
		const bool bAlreadyExists = UnorderedDiscoveredAnchors.ContainsByPredicate(
			[&Incoming](const FOculusXRAnchorsDiscoverResult& Existing)
			{
				return Existing.UUID == Incoming.UUID;
			}
		);

		if (!bAlreadyExists)
		{
			UnorderedDiscoveredAnchors.Add(Incoming);
		}
	}
}

void UAnchorsManagerSubsystem::OnDiscoveryComplete(EOculusXRAnchorResult::Type Result)
{
	if (Result != EOculusXRAnchorResult::Success)
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnchorsManagerSubsystem: Discovery completed with error. Result: %d"), (int32)Result);
	}

	// Rebuild in A/B/C/D order — missing UUIDs get an empty sentinel to preserve slot indices
	TArray<FOculusXRAnchorsDiscoverResult> AnchorsToSpawn;
	for (const FOculusXRUUID& UUID : OrderedUUIDs)
	{
		const FOculusXRAnchorsDiscoverResult* Found = UnorderedDiscoveredAnchors.FindByPredicate(
			[&UUID](const FOculusXRAnchorsDiscoverResult& R) { return R.UUID == UUID; }
		);

		if (!Found)
		{
			UE_LOG(LogTemp, Warning, TEXT("UAnchorsManagerSubsystem: No discovery result for one of the expected UUIDs."));
			AnchorsToSpawn.Add(FOculusXRAnchorsDiscoverResult{}); // empty sentinel — preserves slot index
			continue;
		}

		AnchorsToSpawn.Add(*Found);
	}

	UnorderedDiscoveredAnchors.Reset();

	SpawnRawAnchors(AnchorsToSpawn);
}

void UAnchorsManagerSubsystem::RequestAnchors(const FCustomAnchors& RawAnchors, TFunction<void(TArray<AActor*>)> OnComplete)
{
	// Store callback so SpawnRawAnchors can fire it on the success path
	PendingCallback = OnComplete;

	// Capture ordered UUIDs so results can be matched back to A/B/C/D slots
	const TArray<FOculusXRUUID> WantedUUIDs = {
		RawAnchors.AAnchorUUID,
		RawAnchors.BAnchorUUID,
		RawAnchors.CAnchorUUID,
		RawAnchors.DAnchorUUID
	};

	EOculusXRAnchorResult::Type OutResult;
	const bool bStarted = OculusXRAnchors::FOculusXRAnchors::GetSharedAnchors(
		WantedUUIDs,
		FOculusXRGetSharedAnchorsDelegate::CreateLambda(
			[this, WantedUUIDs](
				EOculusXRAnchorResult::Type Result,
				const TArray<FOculusXRAnchorsDiscoverResult>& RetrievedAnchors)
			{
				if (Result != EOculusXRAnchorResult::Success)
				{
					switch (Result)
					{
					case EOculusXRAnchorResult::Failure_SpaceCloudStorageDisabled:
						UE_LOG(LogTemp, Error, TEXT("UAnchorsManagerSubsystem::RequestAnchors: Enhanced Spatial Services permission is not enabled. Enable it in the Meta developer portal."));
						break;
					case EOculusXRAnchorResult::Failure_SpaceNetworkTimeout:
						UE_LOG(LogTemp, Warning, TEXT("UAnchorsManagerSubsystem::RequestAnchors: Network timeout. Check connectivity and retry."));
						break;
					case EOculusXRAnchorResult::Failure_SpaceNetworkRequestFailed:
						UE_LOG(LogTemp, Warning, TEXT("UAnchorsManagerSubsystem::RequestAnchors: Network request failed. Check connectivity."));
						break;
					case EOculusXRAnchorResult::Failure_SpacePermissionInsufficient:
						UE_LOG(LogTemp, Error, TEXT("UAnchorsManagerSubsystem::RequestAnchors: Insufficient permissions for shared anchors."));
						break;
					case EOculusXRAnchorResult::Failure_SpaceRateLimited:
						UE_LOG(LogTemp, Warning, TEXT("UAnchorsManagerSubsystem::RequestAnchors: Rate limited. Retry after a delay."));
						break;
					default:
						UE_LOG(LogTemp, Error, TEXT("UAnchorsManagerSubsystem::RequestAnchors: Failed with result code: %d"), (int32)Result);
						break;
					}

					if (PendingCallback) { PendingCallback({}); PendingCallback = nullptr; }
					return;
				}

				// Rebuild in A/B/C/D order — missing UUIDs get an empty sentinel to preserve slot indices
				TArray<FOculusXRAnchorsDiscoverResult> AnchorsToSpawn;
				for (const FOculusXRUUID& UUID : WantedUUIDs)
				{
					const FOculusXRAnchorsDiscoverResult* Found = RetrievedAnchors.FindByPredicate(
						[&UUID](const FOculusXRAnchorsDiscoverResult& R) { return R.UUID == UUID; }
					);

					if (!Found)
					{
						UE_LOG(LogTemp, Warning, TEXT("UAnchorsManagerSubsystem::RequestAnchors: No result for one of the expected UUIDs. Inserting sentinel to preserve index."));
						AnchorsToSpawn.Add(FOculusXRAnchorsDiscoverResult{}); // empty sentinel — preserves slot index
						continue;
					}

					AnchorsToSpawn.Add(*Found);
				}

				SpawnRawAnchors(AnchorsToSpawn);
			}
		),
		OutResult
	);

	if (!bStarted)
	{
		UE_LOG(LogTemp, Error, TEXT("UAnchorsManagerSubsystem::RequestAnchors: Failed to start request. Result: %d"), (int32)OutResult);
		if (PendingCallback) { PendingCallback({}); PendingCallback = nullptr; }
	}
}

void UAnchorsManagerSubsystem::SpawnRawAnchors(const TArray<FOculusXRAnchorsDiscoverResult>& RawAnchorsToSpawn)
{
	if (!AnchorClass)
	{
		UE_LOG(LogTemp, Error, TEXT("UAnchorsManagerSubsystem: AnchorClass is null."));
		if (PendingCallback) { PendingCallback({}); PendingCallback = nullptr; }
		return;
	}

	TArray<AActor*> SpawnedAnchors;

	for (const FOculusXRAnchorsDiscoverResult& AnchorData : RawAnchorsToSpawn)
	{
		// Empty sentinel (UUID not found during ordering) — preserve the nullptr slot
		if (!AnchorData.UUID.IsValidUUID())
		{
			SpawnedAnchors.Add(nullptr);
			continue;
		}

		AActor* SpawnedAnchor = UOculusXRAnchorBPFunctionLibrary::SpawnActorWithAnchorHandle(
			GetWorld(),
			AnchorData.Space,
			AnchorData.UUID,
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

		// Always add — nullptr preserved so caller indices stay aligned with A/B/C/D
		SpawnedAnchors.Add(SpawnedAnchor);
	}

	if (SpawnedAnchors.Num() > 0 && SpawnedAnchors[0])
	{
		BaseAnchor = SpawnedAnchors[0];
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnchorsManagerSubsystem: No Anchors were spawned or anchor A is missing."));
	}

	// Fire callback with actors in A(0) B(1) C(2) D(3) order
	if (PendingCallback)
	{
		PendingCallback(SpawnedAnchors);
		PendingCallback = nullptr;
	}
}

AActor* UAnchorsManagerSubsystem::GetBaseAnchor()
{
	return BaseAnchor;
}
