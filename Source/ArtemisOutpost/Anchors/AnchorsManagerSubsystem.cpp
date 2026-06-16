// Fill out your copyright notice in the Description page of Project Settings.

#include "AnchorsManagerSubsystem.h"

#include "ArtemisAnchorSettings.h"
#include "OculusXRAnchorBPFunctionLibrary.h"
#include "OculusXRAnchorComponent.h"
#include "OculusXRAnchors.h"
#include "OculusXRAnchorsRequests.h"

void UAnchorsManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	DiscoveredAnchorDelegate.BindUObject(this, &UAnchorsManagerSubsystem::OnAnchorDiscovered);
	DiscoveredAnchorsCompleteDelegate.BindUObject(this, &UAnchorsManagerSubsystem::OnDiscoveryComplete);

	const UArtemisAnchorSettings* Settings = GetDefault<UArtemisAnchorSettings>();
	AnchorClass = Settings->SpatialAnchorModelClass.LoadSynchronous();

	if (!AnchorClass)
	{
		UE_LOG(LogTemp, Error, TEXT("UAnchorsManagerSubsystem: SpatialAnchorModelClass is not set in Project Settings."));
	}

	// Fixed group UUID for anchor sharing — all devices in the same app use this group.
	// 32-char hex, no hyphens: "5d7649e4-9b66-426e-81df-3db339fd0ddd" → "5d7649e49b66426e81df3db339fd0ddd"
	SharingGroupUUID = UOculusXRAnchorBPFunctionLibrary::StringToAnchorUUID(TEXT("5d7649e49b66426e81df3db339fd0ddd"));
}

// ────────────────────────────────────────────────────────────────────
//  Local Discovery (host discovers anchors already on this device)
// ────────────────────────────────────────────────────────────────────

void UAnchorsManagerSubsystem::DiscoverAnchors(const FOrderedAnchors& RawAnchors, TFunction<void(TArray<AActor*>&)> OnComplete)
{
	if (PendingCallback)
	{
		UE_LOG(LogTemp, Warning, TEXT("DiscoverAnchors: already in progress, switching to new."));
	}
	
	UE_LOG(LogTemp, Log, TEXT("AnchorsManagerSubsystem: Discovering anchors..."))
	
	++CallsCountToDiscover; 
	PendingCallback = OnComplete;
	UnorderedDiscoveredAnchors.Reset();
	OrderedUUIDs = {
		RawAnchors.AAnchorUUID,
		RawAnchors.BAnchorUUID,
		RawAnchors.CAnchorUUID,
		RawAnchors.DAnchorUUID
	};

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
		UE_LOG(LogTemp, Warning, TEXT("DiscoverAnchors: Failed to start. Result: %d"), (int32)OutResult);
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
		LogAnchorError(TEXT("OnDiscoveryComplete"), Result);
		return; 
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
			UE_LOG(LogTemp, Warning, TEXT("OnDiscoveryComplete: No discovery result for one of the expected UUIDs. Aborting."));
			
			if (PendingCallback) 
			{ 
				TArray<AActor*> Empty; 
				PendingCallback(Empty); 
				PendingCallback = nullptr; 
			}
			
			return;  
		}

		AnchorsToSpawn.Add(*Found);
	}
	
	UnorderedDiscoveredAnchors.Reset();
	SpawnRawAnchors(AnchorsToSpawn);
}

// ────────────────────────────────────────────────────────────────────
//  Group Sharing — Host saves + shares anchors with a group
// ────────────────────────────────────────────────────────────────────

void UAnchorsManagerSubsystem::ShareAnchorsWithGroup(const TArray<AActor*>& AnchorActors)
{
	// New share attempt — allow exactly one conclusion for this run.
	bShareConcluded = false;
	
	const FString GroupHex = FGuid::NewGuid().ToString(EGuidFormats::Digits); // 32 hex chars
	SharingGroupUUID = UOculusXRAnchorBPFunctionLibrary::StringToAnchorUUID(GroupHex);

	if (!SharingGroupUUID.IsValidUUID())
	{
		UE_LOG(LogTemp, Error, TEXT("ShareAnchorsWithGroup: SharingGroupUUID is invalid. Check Initialize()."));
		ConcludeShare(false);
		return;
	}

	UE_LOG(LogTemp, Display, TEXT("AnchorsManagerSubsystem: Sharing %d anchor(s)."), AnchorActors.Num());
	SuccessfullySavedAnchorsToCloud.Empty();

	// Extract anchor components from the spawned actors
	TArray<UOculusXRAnchorComponent*> AnchorComponents;
	for (AActor* Actor : AnchorActors)
	{
		if (!IsValid(Actor))
		{
			UE_LOG(LogTemp, Warning, TEXT("ShareAnchorsWithGroup: Skipping null/invalid actor."));
			continue;
		}

		UOculusXRAnchorComponent* Comp = Actor->FindComponentByClass<UOculusXRAnchorComponent>();
		if (!Comp || !Comp->HasValidHandle())
		{
			UE_LOG(LogTemp, Warning, TEXT("ShareAnchorsWithGroup: Actor '%s' has no valid anchor component. Skipping."),
				*Actor->GetName());
			continue;
		}
		
		UE_LOG(LogTemp, Display, TEXT("AnchorsManagerSS: Valid UUID to share: %s"), *Comp->GetUUID().ToString());
		AnchorComponents.Add(Comp);
	}

	if (AnchorComponents.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("ShareAnchorsWithGroup: No valid anchor components found. Nothing to share."));
		ConcludeShare(false);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("ShareAnchorsWithGroup: Saving %d anchors before sharing..."), AnchorComponents.Num());

	SaveAnchorsToCloud(AnchorComponents);
}

void UAnchorsManagerSubsystem::SaveAnchorsToCloud(TArray<UOculusXRAnchorComponent*>& AnchorComponents)
{
	// Arm a single timeout covering the whole save+share flow. If any SDK callback is never
	// delivered (a dropped save, or a share-complete event that never arrives), this guarantees
	// the flow still concludes instead of hanging forever.
	PendingSaveCount = AnchorComponents.Num();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ShareTimeoutTimer, this, &UAnchorsManagerSubsystem::OnShareTimeout, ShareTimeoutSec, false);
	}

	for (auto* AnchorComponent : AnchorComponents)
	{
		EOculusXRAnchorResult::Type SaveResult;

		const bool bSaveStarted = OculusXRAnchors::FOculusXRAnchors::SaveAnchor(
			AnchorComponent,
			EOculusXRSpaceStorageLocation::Cloud,
			FOculusXRAnchorSaveDelegate::CreateLambda([this, AnchorComponents](EOculusXRAnchorResult::Type Result, UOculusXRAnchorComponent* SavedAnchor)
				{
					if (Result != EOculusXRAnchorResult::Success)
					{
						UE_LOG(LogTemp, Error, TEXT("OnSavedAnchor: Failed to save anchor with UUID: %s to cloud. Result: %d"), *SavedAnchor->GetUUID().ToString(), (int32)Result);

						SuccessfullySavedAnchorsToCloud.Empty();
						ConcludeShare(false);

						return;
					}

					SuccessfullySavedAnchorsToCloud.AddUnique(SavedAnchor);

					if (SuccessfullySavedAnchorsToCloud.Num() == AnchorComponents.Num())
					{
						OnAnchorsSaved(Result, SuccessfullySavedAnchorsToCloud);
					}
				}
			),
			SaveResult
		);

		if (!bSaveStarted)
		{
			UE_LOG(LogTemp, Error, TEXT("SaveAnchorsToCloud: Failed to start anchor save. Result: %d"), (int32)SaveResult);

			SuccessfullySavedAnchorsToCloud.Empty();
			ConcludeShare(false);

			return;
		}
	}
}

void UAnchorsManagerSubsystem::OnAnchorsSaved(EOculusXRAnchorResult::Type Result, const TArray<UOculusXRAnchorComponent*>& SavedAnchors)
{
	if (Result != EOculusXRAnchorResult::Success)
	{
		LogAnchorError(TEXT("ShareAnchorsWithGroup [Save]"), Result);
		ConcludeShare(false);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("ShareAnchorsWithGroup: Saved %d anchors. Starting share..."), SavedAnchors.Num());

	// Step 2 — Collect handles from the saved components
	TArray<FOculusXRUInt64> AnchorHandles;
	for (const UOculusXRAnchorComponent* Comp : SavedAnchors)
	{
		if (IsValid(Comp))
		{
			AnchorHandles.Add(Comp->GetHandle());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ShareAnchorsWithGroup: Not a valid anchor handle. Skipping."));
		}
	}

	if (AnchorHandles.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("ShareAnchorsWithGroup: No valid handles after save. Cannot share."));
		ConcludeShare(false);
		return;
	}
	
	UE_LOG(LogTemp, Log, TEXT("ShareAnchorsWithGroup: Sharing %d anchor(s)..."), AnchorHandles.Num());

	// Step 3 — Share with group via the recommended async API
	const TArray<FOculusXRUUID> Groups = { SharingGroupUUID };

	TSharedPtr<OculusXRAnchors::FShareAnchorsWithGroups> ShareRequest =
		OculusXRAnchors::FOculusXRAnchors::ShareAnchorsAsync(
			AnchorHandles,
			Groups,
			OculusXRAnchors::FShareAnchorsWithGroups::FCompleteDelegate::CreateLambda(
				[this](const OculusXRAnchors::FShareAnchorsWithGroups::FResultType& ShareResult)
				{
					if (ShareResult.IsSuccess())
					{
						UE_LOG(LogTemp, Log, TEXT("ShareAnchorsWithGroup: Anchors shared successfully with group."));
						ConcludeShare(true);
					}
					else
					{
						LogAnchorError(TEXT("ShareAnchorsWithGroup [Share]"), ShareResult.GetStatus());
						ConcludeShare(false);
					}
				}
			)
		);

	if (!ShareRequest.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("ShareAnchorsWithGroup: Failed to create share request."));
		ConcludeShare(false);
	}
}

void UAnchorsManagerSubsystem::ConcludeShare(bool bSuccess)
{
	// Fire-once guard: any of the save/share failure paths (or a timeout) may try to conclude;
	// only the first one is allowed to broadcast a result for this share attempt.
	if (bShareConcluded)
	{
		return;
	}
	bShareConcluded = true;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ShareTimeoutTimer);
	}

	OnAnchorsSharedResult.Broadcast(bSuccess);
}

void UAnchorsManagerSubsystem::OnShareTimeout()
{
	UE_LOG(LogTemp, Warning,
		TEXT("ShareAnchorsWithGroup: Timed out after %.0fs waiting for save/share completion (%d/%d anchors saved). Aborting."),
		ShareTimeoutSec, SuccessfullySavedAnchorsToCloud.Num(), PendingSaveCount);

	SuccessfullySavedAnchorsToCloud.Empty();
	ConcludeShare(false);
}

// ────────────────────────────────────────────────────────────────────
//  Group Retrieval — Client fetches shared anchors and spawns actors
// ────────────────────────────────────────────────────────────────────

void UAnchorsManagerSubsystem::RequestSharedAnchors(const FOrderedAnchors& RawAnchors, TFunction<void(TArray<AActor*>&)> OnComplete)
{
	if (PendingCallback)
	{
		UE_LOG(LogTemp, Warning, TEXT("DiscoverAnchors: already in progress, ignoring."));
		return;
	}
	
	PendingCallback = OnComplete;

	if (!SharingGroupUUID.IsValidUUID())
	{
		UE_LOG(LogTemp, Error, TEXT("RequestSharedAnchors: SharingGroupUUID is invalid. Check Initialize()."));
		if (PendingCallback) 
		{ 
			TArray<AActor*> Empty; 
			PendingCallback(Empty); 
			PendingCallback = nullptr; 
		}
		return;
	}

	OrderedUUIDs = {
		RawAnchors.AAnchorUUID,
		RawAnchors.BAnchorUUID,
		RawAnchors.CAnchorUUID,
		RawAnchors.DAnchorUUID
	};

	UE_LOG(LogTemp, Log, TEXT("RequestSharedAnchors: Requesting anchors from group..."));
	
	EOculusXRAnchorResult::Type OutResult;
	bool bResult = OculusXRAnchors::FOculusXRAnchors::GetSharedAnchors(
		OrderedUUIDs,
		FOculusXRGetSharedAnchorsDelegate::CreateLambda(
			[this](EOculusXRAnchorResult::Type Result, const TArray<FOculusXRAnchorsDiscoverResult>& RetrievedAnchors)
			{
				if (Result != EOculusXRAnchorResult::Success)
				{
					LogAnchorError(TEXT("RequestSharedAnchors"), Result);
					if (PendingCallback)
					{
						TArray<AActor*> Empty; 
						PendingCallback(Empty); 
						PendingCallback = nullptr;
					}
					return;
				}
				
				// Rebuild in A/B/C/D order — convert FOculusXRAnchor to FOculusXRAnchorsDiscoverResult for SpawnRawAnchors
					TArray<FOculusXRAnchorsDiscoverResult> OrderedAnchorsToSpawn;
					for (const FOculusXRUUID& UUID : OrderedUUIDs)
					{
						const FOculusXRAnchorsDiscoverResult* Found = RetrievedAnchors.FindByPredicate([&UUID](const FOculusXRAnchorsDiscoverResult& A) { return A.UUID == UUID; });
						
						if (!Found)
						{
							UE_LOG(LogTemp, Warning, TEXT("RequestSharedAnchors: UUID not found in results. Inserting sentinel."));
							OrderedAnchorsToSpawn.Add(FOculusXRAnchorsDiscoverResult{});
							continue;
						}

						// FOculusXRAnchor::AnchorHandle maps to FOculusXRAnchorsDiscoverResult::Space
						OrderedAnchorsToSpawn.Add(FOculusXRAnchorsDiscoverResult(Found->Space, Found->UUID));
					}

					SpawnRawAnchors(OrderedAnchorsToSpawn);
				
			}
		),
		OutResult
	);  
	
	if (OutResult != EOculusXRAnchorResult::Success)
	{
		UE_LOG(LogTemp, Error, TEXT("RequestSharedAnchors: Failed to create async request."));
		if (PendingCallback) 
		{ 
			TArray<AActor*> Empty; 
			PendingCallback(Empty); 
			PendingCallback = nullptr; 
		}
	}
	
	/*
	const TSharedPtr<OculusXRAnchors::FGetAnchorsSharedWithGroup> Request =
		OculusXRAnchors::FOculusXRAnchors::GetSharedAnchorsAsync(
			SharingGroupUUID,
			OrderedUUIDs,
			OculusXRAnchors::FGetAnchorsSharedWithGroup::FCompleteDelegate::CreateLambda(
				[this](const OculusXRAnchors::FGetAnchorsSharedWithGroup::FResultType& Result) 
				{
					if (!Result.IsSuccess())
					{
						LogAnchorError(TEXT("RequestSharedAnchors"), Result.GetStatus());
						if (PendingCallback)
						{
							TArray<AActor*> Empty; 
							PendingCallback(Empty); 
							PendingCallback = nullptr;
						}
						return;
					}

					const TArray<FOculusXRAnchor>& RetrievedAnchors = Result.GetValue();
					UE_LOG(LogTemp, Log, TEXT("RequestSharedAnchors: Retrieved %d anchor(s). Ordering and spawning..."),
						RetrievedAnchors.Num());

					// Rebuild in A/B/C/D order — convert FOculusXRAnchor to FOculusXRAnchorsDiscoverResult for SpawnRawAnchors
					TArray<FOculusXRAnchorsDiscoverResult> OrderedAnchorsToSpawn;
					for (const FOculusXRUUID& UUID : OrderedUUIDs)
					{
						const FOculusXRAnchor* Found = RetrievedAnchors.FindByPredicate([&UUID](const FOculusXRAnchor& A) { return A.Uuid == UUID; });
						
						if (!Found)
						{
							UE_LOG(LogTemp, Warning, TEXT("RequestSharedAnchors: UUID not found in results. Inserting sentinel."));
							OrderedAnchorsToSpawn.Add(FOculusXRAnchorsDiscoverResult{});
							continue;
						}

						// FOculusXRAnchor::AnchorHandle maps to FOculusXRAnchorsDiscoverResult::Space
						OrderedAnchorsToSpawn.Add(FOculusXRAnchorsDiscoverResult(Found->AnchorHandle, Found->Uuid));
					}

					SpawnRawAnchors(OrderedAnchorsToSpawn);
				}
			)
		);

	if (!Request.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("RequestSharedAnchors: Failed to create async request."));
		if (PendingCallback) 
		{ 
			TArray<AActor*> Empty; 
			PendingCallback(Empty); 
			PendingCallback = nullptr; 
		}
	}
	*/
}

// ────────────────────────────────────────────────────────────────────
//  Spawning — shared by both discovery and retrieval paths
// ────────────────────────────────────────────────────────────────────

void UAnchorsManagerSubsystem::SpawnRawAnchors(const TArray<FOculusXRAnchorsDiscoverResult>& RawOrderedAnchorsToSpawn)
{
	++CallsCountToSpawn;
	if (CallsCountToDiscover != CallsCountToSpawn)
	{
		UE_LOG(LogTemp, Log, TEXT("SpawnRawAnchors: Attempted to spawn stale anchors. Aborting."));
		return;
	}
	
	if (!AnchorClass)
	{
		UE_LOG(LogTemp, Error, TEXT("SpawnRawAnchors: AnchorClass is null. Set SpatialAnchorModelClass in Project Settings."));
		if (PendingCallback)
		{
			TArray<AActor*> Empty; 
			PendingCallback(Empty);  
			PendingCallback = nullptr;
		}
		return;
	}
	
	RemoveOldAnchors();
	
	UE_LOG(LogTemp, Log, TEXT("AnchorsManagerSubsystem: Spawning %d new raw anchor(s)..."), RawOrderedAnchorsToSpawn.Num());
	
	for (const FOculusXRAnchorsDiscoverResult& AnchorData : RawOrderedAnchorsToSpawn)
	{
		// Empty sentinel (UUID not found during ordering) — preserve the nullptr slot
		if (!AnchorData.UUID.IsValidUUID())
		{
			SpawnedAnchors.Add(nullptr);
			continue;
		}
		
		UE_LOG(LogTemp, Display, TEXT("AnchorsManagerSubsystem: UUID to spawn: %s"), *AnchorData.UUID.ToString());

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
			SpawnedAnchor->SetActorHiddenInGame(false);
			SpawnedAnchors.Add(SpawnedAnchor);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("SpawnRawAnchors: SpawnActorWithAnchorHandle returned null for UUID: %s"), *AnchorData.UUID.ToString());
			if (PendingCallback)
			{
				TArray<AActor*> Empty; 
				PendingCallback(Empty); 
				PendingCallback = nullptr;
			}
			return; 
		}
	}

	if (SpawnedAnchors.Num() > 0 && SpawnedAnchors[0])
	{
		BaseAnchor = SpawnedAnchors[0];
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("SpawnRawAnchors: No anchors spawned or anchor A is missing."));
	}

	if (PendingCallback)
	{
		// Don't fire the callback yet — the XR runtime updates anchor transforms on the
		// next tick, so GetActorLocation() returns (0,0,0) at this point even though the
		// actors are visually in the right place. Poll until all anchors are localized.
		LocatedPollAttempts = 0;
		const int32 CallCountID = CallsCountToSpawn; 
		WaitForAnchorsLocated(CallCountID);
	}
}

void UAnchorsManagerSubsystem::WaitForAnchorsLocated(const int32 CallCount)
{
	// Check whether every valid (non-sentinel) anchor has been moved off the world origin by
	// the XR runtime tick. GetActorLocation() == zero means the anchor component hasn't received
	// its first pose update yet. Works for both local (DiscoverAnchors) and cloud
	// (RequestSharedAnchors) paths without depending on any specific runtime API.
	bool bAllLocated = true;
	for (AActor* Anchor : SpawnedAnchors)
	{
		if (!IsValid(Anchor)) continue; // nullptr sentinel — skip

		if (Anchor->GetActorLocation().IsNearlyZero())
		{
			UE_LOG(LogTemp, Log, TEXT("AnchorsManagerSubsystem: Found Anchor to close to 0,0,0. Aborting."));
			bAllLocated = false;
			break;
		}
	}

	if (bAllLocated)
	{
		UE_LOG(LogTemp, Log, TEXT("AnchorsManagerSubsystem: All anchors located after %d poll(s)."),
			LocatedPollAttempts + 1);

		GetWorld()->GetTimerManager().ClearTimer(LocatedPollTimer);

		if (PendingCallback)
		{
			if (CallsCountToDiscover != CallCount)
			{
				UE_LOG(LogTemp, Log, TEXT("AnchorsManagerSubsystem: WaitForAnchorsLocated: Aborting these anchors, because fresh anchors incoming."));
				return; 
			}
			
			PendingCallback(SpawnedAnchors);
			PendingCallback = nullptr;
		}
		return;
	}

	/*--------------------- If not all anchors are yet located ---------------------*/
	++LocatedPollAttempts;

	if (LocatedPollAttempts >= LocatedPollMaxAttempts)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("AnchorsManagerSubsystem: Timed out waiting for anchor localization after %d attempts (~%.1f s). "
			     "Firing callback with partially-located anchors."),
			LocatedPollAttempts,
			LocatedPollAttempts * LocatedPollIntervalSec);

		GetWorld()->GetTimerManager().ClearTimer(LocatedPollTimer);

		if (PendingCallback)
		{
			if (CallsCountToDiscover != CallCount)
			{
				UE_LOG(LogTemp, Log, TEXT("AnchorsManagerSubsystem: WaitForAnchorsLocated: Aborting these anchors, because fresh anchors incoming."));
				return; 
			}
			
			PendingCallback(SpawnedAnchors);
			PendingCallback = nullptr;
		}
		return;
	}
	
	FTimerDelegate TimerDelegate;
	TimerDelegate.BindLambda([this, CallCount]()
	{
		this->WaitForAnchorsLocated(CallCount);
	});

	// Not all located yet — reschedule for the next interval.
	GetWorld()->GetTimerManager().SetTimer(
		LocatedPollTimer,
		TimerDelegate,
		LocatedPollIntervalSec,
		false 
	);
}

// ────────────────────────────────────────────────────────────────────
//  Error Logging
// ────────────────────────────────────────────────────────────────────

void UAnchorsManagerSubsystem::LogAnchorError(const TCHAR* Context, EOculusXRAnchorResult::Type Result)
{
	switch (Result)
	{
	case EOculusXRAnchorResult::Failure_SpaceCloudStorageDisabled:
		UE_LOG(LogTemp, Error,
			TEXT("%s: Enhanced Spatial Services is disabled. Enable at Settings > Privacy > Device Permissions."), Context);
		break;
	case EOculusXRAnchorResult::Failure_SpaceNetworkTimeout:
		UE_LOG(LogTemp, Warning,
			TEXT("%s: Network timeout. Check WiFi connectivity and retry."), Context);
		break;
	case EOculusXRAnchorResult::Failure_SpaceNetworkRequestFailed:
		UE_LOG(LogTemp, Warning,
			TEXT("%s: Network request failed. Device may have lost internet."), Context);
		break;
	case EOculusXRAnchorResult::Failure_SpacePermissionInsufficient:
		UE_LOG(LogTemp, Error,
			TEXT("%s: Insufficient permissions. Verify app is registered and DUC is approved on developer.meta.com."), Context);
		break;
	case EOculusXRAnchorResult::Failure_SpaceRateLimited:
		UE_LOG(LogTemp, Warning,
			TEXT("%s: Rate limited by Meta cloud. Retry after a delay."), Context);
		break;
	case EOculusXRAnchorResult::Failure_SpaceMappingInsufficient:
		UE_LOG(LogTemp, Warning,
			TEXT("%s: Insufficient spatial mapping. Prompt user to look around the room."), Context);
		break;
	case EOculusXRAnchorResult::Failure_SpaceLocalizationFailed:
		UE_LOG(LogTemp, Warning,
			TEXT("%s: Localization failed. Anchor data does not match the local environment."), Context);
		break;
	case EOculusXRAnchorResult::Failure_InvalidParameter:
		UE_LOG(LogTemp, Error,
			TEXT("%s: Invalid parameter. Check anchor handles and UUID format (32-char hex, no hyphens)."), Context);
		break;
	default:
		UE_LOG(LogTemp, Error,
			TEXT("%s: Failed with result code %d."), Context, (int32)Result);
		break;
	}
}

void UAnchorsManagerSubsystem::RemoveOldAnchors()
{
	for (AActor* Anchor : SpawnedAnchors)
	{
		if (IsValid(Anchor))
		{
			Anchor->Destroy();
		}
	}
	SpawnedAnchors.Reset();  
}

// ────────────────────────────────────────────────────────────────────
//  Getters
// ────────────────────────────────────────────────────────────────────

AActor* UAnchorsManagerSubsystem::GetBaseAnchor()
{
	return BaseAnchor;
}

TArray<AActor*> UAnchorsManagerSubsystem::GetAnchors()
{
	return SpawnedAnchors;
}
