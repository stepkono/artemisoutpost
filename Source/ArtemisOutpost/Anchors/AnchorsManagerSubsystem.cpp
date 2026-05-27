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
	SavedAnchorsDelegate.BindUObject(this, &UAnchorsManagerSubsystem::OnAnchorsSaved);

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

void UAnchorsManagerSubsystem::DiscoverAnchors(const FOrderedAnchors& RawAnchors, TFunction<void(TArray<AActor*>)> OnComplete)
{
	if (PendingCallback)
	{
		UE_LOG(LogTemp, Warning, TEXT("DiscoverAnchors: already in progress, ignoring."));
		return;
	}
	
	UE_LOG(LogTemp, Log, TEXT("AnchorsManagerSubsystem: Discovering anchors..."))
	
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
			UE_LOG(LogTemp, Warning, TEXT("OnDiscoveryComplete: No discovery result for one of the expected UUIDs."));
			AnchorsToSpawn.Add(FOculusXRAnchorsDiscoverResult{});
			continue;
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
	if (!SharingGroupUUID.IsValidUUID())
	{
		UE_LOG(LogTemp, Error, TEXT("ShareAnchorsWithGroup: SharingGroupUUID is invalid. Check Initialize()."));
		OnAnchorsSharedResult.Broadcast(false);
		return;
	}

	UE_LOG(LogTemp, Display, TEXT("AnchorsManagerSubsystem: Sharing %d anchor(s)."), AnchorActors.Num()); 

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
		OnAnchorsSharedResult.Broadcast(false);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("ShareAnchorsWithGroup: Saving %d anchors before sharing..."), AnchorComponents.Num());

	// Step 1 — Save anchors (persists them so the cloud share can reference them)
	EOculusXRAnchorResult::Type SaveResult;
	const bool bSaveStarted = OculusXRAnchors::FOculusXRAnchors::SaveAnchors(AnchorComponents, SavedAnchorsDelegate, SaveResult);

	if (!bSaveStarted)
	{
		UE_LOG(LogTemp, Error, TEXT("ShareAnchorsWithGroup: Failed to start anchor save. Result: %d"), (int32)SaveResult);
		OnAnchorsSharedResult.Broadcast(false);
	}
}

void UAnchorsManagerSubsystem::OnAnchorsSaved(EOculusXRAnchorResult::Type Result, const TArray<UOculusXRAnchorComponent*>& SavedAnchors)
{
	if (Result != EOculusXRAnchorResult::Success)
	{
		LogAnchorError(TEXT("ShareAnchorsWithGroup [Save]"), Result);
		OnAnchorsSharedResult.Broadcast(false);
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
		OnAnchorsSharedResult.Broadcast(false);
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
						OnAnchorsSharedResult.Broadcast(true);
					}
					else
					{
						LogAnchorError(TEXT("ShareAnchorsWithGroup [Share]"), ShareResult.GetStatus());
						OnAnchorsSharedResult.Broadcast(false);
					}
				}
			)
		);

	if (!ShareRequest.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("ShareAnchorsWithGroup: Failed to create share request."));
		OnAnchorsSharedResult.Broadcast(false);
	}
}

// ────────────────────────────────────────────────────────────────────
//  Group Retrieval — Client fetches shared anchors and spawns actors
// ────────────────────────────────────────────────────────────────────

void UAnchorsManagerSubsystem::RequestSharedAnchors(const FOrderedAnchors& RawAnchors, TFunction<void(TArray<AActor*>)> OnComplete)
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
			PendingCallback({}); 
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
							PendingCallback({}); 
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
			PendingCallback({}); 
			PendingCallback = nullptr; 
		}
	}
}

// ────────────────────────────────────────────────────────────────────
//  Spawning — shared by both discovery and retrieval paths
// ────────────────────────────────────────────────────────────────────

void UAnchorsManagerSubsystem::SpawnRawAnchors(const TArray<FOculusXRAnchorsDiscoverResult>& RawOrderedAnchorsToSpawn)
{
	if (!AnchorClass)
	{
		UE_LOG(LogTemp, Error, TEXT("SpawnRawAnchors: AnchorClass is null. Set SpatialAnchorModelClass in Project Settings."));
		if (PendingCallback)
		{
			PendingCallback({}); 
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
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("SpawnRawAnchors: SpawnActorWithAnchorHandle returned null for UUID: %s"), *AnchorData.UUID.ToString());
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
		UE_LOG(LogTemp, Warning, TEXT("SpawnRawAnchors: No anchors spawned or anchor A is missing."));
	}

	if (PendingCallback)
	{
		// Don't fire the callback yet — the XR runtime updates anchor transforms on the
		// next tick, so GetActorLocation() returns (0,0,0) at this point even though the
		// actors are visually in the right place. Poll until all anchors are localized.
		LocatedPollAttempts = 0;
		WaitForAnchorsLocated();
	}
}

void UAnchorsManagerSubsystem::WaitForAnchorsLocated()
{
	// Check whether every valid (non-sentinel) anchor has been localized by the XR runtime.
	// GetAnchorTransformByHandle() is the same call TickComponent uses internally — it returns
	// false if the runtime hasn't resolved the pose yet, true once it has.
	bool bAllLocated = true;
	for (AActor* Anchor : SpawnedAnchors)
	{
		if (!IsValid(Anchor)) continue; // nullptr sentinel — skip

		const UOculusXRAnchorComponent* Comp = Anchor->FindComponentByClass<UOculusXRAnchorComponent>();
		if (!Comp || !Comp->HasValidHandle())
		{
			bAllLocated = false;
			break;
		}

		FTransform OutTransform;
		if (!UOculusXRAnchorBPFunctionLibrary::GetAnchorTransformByHandle(Comp->GetHandle(), OutTransform))
		{
			bAllLocated = false;
			break;
		}
		
		UE_LOG(LogTemp, Warning, TEXT("WaitForAnchorsLocated: Got anchor location: %s"), *Anchor->GetActorLocation().ToString());
	}

	if (bAllLocated)
	{
		UE_LOG(LogTemp, Log, TEXT("AnchorsManagerSubsystem: All anchors located after %d poll(s)."),
			LocatedPollAttempts + 1);

		GetWorld()->GetTimerManager().ClearTimer(LocatedPollTimer);

		if (PendingCallback)
		{
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
			PendingCallback(SpawnedAnchors);
			PendingCallback = nullptr;
		}
		return;
	}

	// Not all located yet — reschedule for the next interval.
	GetWorld()->GetTimerManager().SetTimer(
		LocatedPollTimer,
		this,
		&UAnchorsManagerSubsystem::WaitForAnchorsLocated,
		LocatedPollIntervalSec,
		false // one-shot; we re-arm manually so there's no risk of a stale loop
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
