// Fill out your copyright notice in the Description page of Project Settings.

#include "AnchorsManagerSubsystem.h"

#include "ArtemisAnchorSettings.h"
#include "OculusXRAnchorBPFunctionLibrary.h"
#include "OculusXRAnchorComponent.h"
#include "OculusXRAnchors.h"
#include "OculusXRAnchorsRequests.h"
#include "ArtemisOutpost/GameData/ArtemisGameState.h"
#include "ArtemisOutpost/Moon/Cesium/GeoTools/GeoUtils.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "IXRTrackingSystem.h"

// How many times the anchor actors carry the XR base orientation (the VR surface tilt). HMD and controllers carry it
// once: Epic's OpenXR plugin bakes the base pose into the tracking space. MetaXR's FAnchorsXR::TryGetAnchorTransform
// (Plugins/MetaXR/Source/OculusXRAnchors/Private/openxr/OculusXRAnchorsXR.cpp) locates the anchor in that already
// rotated space and applies Base^-1 a SECOND time by hand. Verified on device 2026-09-24 (80 deg tilt). Set to 1 once
// the plugin is patched or fixed upstream, the presenter's "AnchorsTilt" log line reports the current value.
static TAutoConsoleVariable<int32> CVarAnchorTiltApplications(
	TEXT("artemis.AnchorTiltApplications"),
	2,
	TEXT("How many times the spatial anchor actors carry the XR base orientation (MetaXR double-applies it: 2)."),
	ECVF_Default);

namespace
{
	// The rotation the XR runtime applies to tracked device poses (the VR surface tilt). Identity in AR.
	FQuat GetTrackedDeviceTilt()
	{
		return (GEngine && GEngine->XRSystem.IsValid()) ? GEngine->XRSystem->GetBaseOrientation().Inverse() : FQuat::Identity;
	}

	FTransform GetTrackingToWorld()
	{
		return (GEngine && GEngine->XRSystem.IsValid()) ? GEngine->XRSystem->GetTrackingToWorldTransform() : FTransform::Identity;
	}

	int32 GetAnchorTiltApplications()
	{
		return FMath::Clamp(CVarAnchorTiltApplications.GetValueOnGameThread(), 0, 3);
	}

	// Tilt^Power, a negative power composes the inverse.
	FQuat TiltPower(const FQuat& Tilt, int32 Power)
	{
		FQuat Result = FQuat::Identity;
		for (int32 Index = 0; Index < FMath::Abs(Power); ++Index)
		{
			Result = (Power > 0 ? Tilt : Tilt.Inverse()) * Result;
		}
		return Result;
	}

	// Rotates in TRACKING space (around the tracking origin), where the base orientation acts. Exact even if
	// TrackingToWorld carries a rotation.
	FVector RotatePositionInTrackingSpace(const FVector& WorldPosition, const FQuat& Rotation, const FTransform& TrackingToWorld)
	{
		return TrackingToWorld.TransformPosition(Rotation.RotateVector(TrackingToWorld.InverseTransformPosition(WorldPosition)));
	}

	FQuat RotateOrientationInTrackingSpace(const FQuat& WorldOrientation, const FQuat& Rotation, const FTransform& TrackingToWorld)
	{
		const FQuat TrackingRotation = TrackingToWorld.GetRotation();
		return TrackingRotation * Rotation * TrackingRotation.Inverse() * WorldOrientation;
	}
}

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

	// Generate a fresh group UUID for THIS share session. A new group per session keeps the
	// receiver's group query result set small (only this session's anchors), which avoids the
	// XR_ERROR_LIMIT_REACHED accumulation that comes from reusing one fixed group across runs.
	const FString GroupHex = FGuid::NewGuid().ToString(EGuidFormats::Digits); // 32 hex chars, no hyphens
	SharingGroupUUID = UOculusXRAnchorBPFunctionLibrary::StringToAnchorUUID(GroupHex);

	if (!SharingGroupUUID.IsValidUUID())
	{
		UE_LOG(LogTemp, Error, TEXT("ShareAnchorsWithGroup: Failed to generate a valid session group UUID."));
		ConcludeShare(false);
		return;
	}

	// Stash the session group UUID on the GameInstance so the authoritative pawn can replicate
	// it to the other clients (they need it to query the same group).
	if (UArtemisGameInstance* ArtemisGI = Cast<UArtemisGameInstance>(GetGameInstance()))
	{
		ArtemisGI->SharingGroupUUID = SharingGroupUUID;
		UE_LOG(LogTemp, Log, TEXT("ShareAnchorsWithGroup: Session group UUID: %s"), *SharingGroupUUID.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ShareAnchorsWithGroup: Could not cache group UUID on GameInstance (cast failed)."));
	}

	UE_LOG(LogTemp, Display, TEXT("AnchorsManagerSubsystem: Sharing %d anchor(s)."), AnchorActors.Num());

	// Collect valid anchor handles from the spawned actors.
	TArray<FOculusXRUInt64> AnchorHandles;
	for (AActor* Actor : AnchorActors)
	{
		if (!IsValid(Actor))
		{
			UE_LOG(LogTemp, Warning, TEXT("ShareAnchorsWithGroup: Skipping null/invalid actor."));
			continue;
		}

		const UOculusXRAnchorComponent* Comp = Actor->FindComponentByClass<UOculusXRAnchorComponent>();
		if (!Comp || !Comp->HasValidHandle())
		{
			UE_LOG(LogTemp, Warning, TEXT("ShareAnchorsWithGroup: Actor '%s' has no valid anchor component. Skipping."),
				*Actor->GetName());
			continue;
		}

		UE_LOG(LogTemp, Display, TEXT("AnchorsManagerSS: Valid UUID to share: %s"), *Comp->GetUUID().ToString());
		AnchorHandles.Add(Comp->GetHandle());
	}

	if (AnchorHandles.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("ShareAnchorsWithGroup: No valid anchor handles found. Nothing to share."));
		ConcludeShare(false);
		return;
	}

	// Arm a watchdog so the flow still concludes if the share-complete event is never delivered.
	PendingShareCount = AnchorHandles.Num();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ShareTimeoutTimer, this, &UAnchorsManagerSubsystem::OnShareTimeout, ShareTimeoutSec, false);
	}

	UE_LOG(LogTemp, Log, TEXT("ShareAnchorsWithGroup: Sharing %d anchor(s) with group..."), AnchorHandles.Num());

	// Group share (recommended API). The share itself uploads the anchors to the cloud — there is
	// no separate cloud-save step in the group-sharing flow.
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
		TEXT("ShareAnchorsWithGroup: Timed out after %.0fs waiting for share completion (%d anchor(s)). Aborting."),
		ShareTimeoutSec, PendingShareCount);

	ConcludeShare(false);
}

// ────────────────────────────────────────────────────────────────────
//  Group Retrieval — Client fetches shared anchors and spawns actors
// ────────────────────────────────────────────────────────────────────

void UAnchorsManagerSubsystem::RequestSharedAnchors(const FOrderedAnchors& RawAnchors, TFunction<void(TArray<AActor*>&)> OnComplete)
{
	if (PendingCallback)
	{
		UE_LOG(LogTemp, Warning, TEXT("RequestSharedAnchors: already in progress, ignoring."));
		return;
	}

	PendingCallback = OnComplete;
	++CallsCountToDiscover; // pair this request with its SpawnRawAnchors so the stale-spawn guard matches

	// The session group UUID is replicated from the authoritative client via the GameState.
	FOculusXRUUID GroupUUID;
	if (const AArtemisGameState* GS = GetWorld() ? GetWorld()->GetGameState<AArtemisGameState>() : nullptr)
	{
		GroupUUID = GS->GetSharingGroupUUID();
	}

	if (!GroupUUID.IsValidUUID())
	{
		UE_LOG(LogTemp, Error, TEXT("RequestSharedAnchors: No valid session group UUID on the GameState yet."));
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

	UE_LOG(LogTemp, Log, TEXT("RequestSharedAnchors: Requesting anchors from group %s..."), *GroupUUID.ToString());

	const TSharedPtr<OculusXRAnchors::FGetAnchorsSharedWithGroup> Request =
		OculusXRAnchors::FOculusXRAnchors::GetSharedAnchorsAsync(
			GroupUUID,
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

					// Rebuild in A/B/C/D order; missing UUIDs get an empty sentinel to preserve slot indices.
					TArray<FOculusXRAnchorsDiscoverResult> OrderedAnchorsToSpawn;
					for (const FOculusXRUUID& UUID : OrderedUUIDs)
					{
						const FOculusXRAnchor* Found = RetrievedAnchors.FindByPredicate(
							[&UUID](const FOculusXRAnchor& A) { return A.Uuid == UUID; });

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

bool UAnchorsManagerSubsystem::TryGetAnchorsFrame(FTransform& OutFrame) const
{
	if (SpawnedAnchors.Num() < 4 || !IsValid(SpawnedAnchors[0]) || !IsValid(SpawnedAnchors[1]) || !IsValid(SpawnedAnchors[3]))
	{
		UE_LOG(LogTemp, Error, TEXT("[AnchorsManager] TryGetAnchorsFrame: Some anchors are not valid."));
		return false;
	}

	FVector AnchorA = SpawnedAnchors[0]->GetActorLocation();   // A (bottom-left)
	FVector AnchorB = SpawnedAnchors[1]->GetActorLocation();   // B (top-left)
	FVector AnchorD = SpawnedAnchors[3]->GetActorLocation();   // D (bottom-right)

	// ---- VR surface tilt compensation (identity, and skipped, in AR) ----
	// The XR base orientation rotates the tracked poses in TRACKING space (around the tracking origin). HMD and
	// controllers carry it once (Tilt), the anchor actors carry it CVarAnchorTiltApplications times (MetaXR bug, 2).
	// 1. Bring the anchors back to the physical, untilted table, so CalibrateAnchors' world-horizontal flattening
	//    is valid (the physical table is level).
	// 2. Build the frame there.
	// 3. Tilt the finished frame ONCE, into the space the HMD and controllers live in.
	// Result: tracked poses and frame share one space, so the tilt cancels in GetRelativeToAnchorsFrame (physical
	// fractions on the sender) and GetWorldFromAnchorsFrame lands in the tilted camera space (on the receiver).
	// No tilt handling is needed anywhere else, and each client only ever uses its own tilt.
	const FQuat Tilt = GetTrackedDeviceTilt();
	const bool bTilted = !Tilt.Equals(FQuat::Identity, 1.e-6);
	const FTransform TrackingToWorld = bTilted ? GetTrackingToWorld() : FTransform::Identity;
	if (bTilted)
	{
		// Step 1: remove every application the anchor actors carry.
		const FQuat Untilt = TiltPower(Tilt, -GetAnchorTiltApplications());
		AnchorA = RotatePositionInTrackingSpace(AnchorA, Untilt, TrackingToWorld);
		AnchorB = RotatePositionInTrackingSpace(AnchorB, Untilt, TrackingToWorld);
		AnchorD = RotatePositionInTrackingSpace(AnchorD, Untilt, TrackingToWorld);
	}

	// B(top-left)     C(top-right)
	// A(bottom-left)  D(bottom-right)
	const FCalibratedData Calib = UGeoUtils::CalibrateAnchors(AnchorA, AnchorB, AnchorD);

	const FVector XAxis = Calib.BAnchorPos - Calib.AAnchorPos;
	const FVector YAxis = Calib.DAnchorPos - Calib.AAnchorPos;

	// Uniform scale = table edge length. The anchors scale with WorldToMeters, so dividing positions
	// by this edge length turns them into WTM-invariant fractions of the table that line up across
	// colocated clients regardless of each client's zoom level.
	const float EdgeLen = XAxis.Length();
	if (EdgeLen <= KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogTemp, Error, TEXT("[AnchorsManager] TryGetAnchorsFrame: EdgeLength is 0."));
		return false;
	}

	// Orthonormal table orientation. Scale is carried separately, not baked into the basis, so all
	// three axes scale uniformly and the resulting local coordinate is a pure ratio.
	const FMatrix Basis = UGeoUtils::BuildMatrixFromVectors(XAxis, YAxis);

	FQuat   FrameRotation = Basis.ToQuat();
	FVector FrameCenter   = Calib.PlaneCenter;
	if (bTilted)
	{
		// Step 3: physical table -> the tilted space of the HMD and controllers (one application, in tracking space).
		FrameCenter   = RotatePositionInTrackingSpace(FrameCenter, Tilt, TrackingToWorld);
		FrameRotation = RotateOrientationInTrackingSpace(FrameRotation, Tilt, TrackingToWorld);
	}

	OutFrame = FTransform(FrameRotation, FrameCenter, FVector(EdgeLen));
	return true;
}

FTransform UAnchorsManagerSubsystem::RawAnchorToTrackedSpace(const FTransform& RawAnchorTransform)
{
	const FQuat Tilt = GetTrackedDeviceTilt();
	if (Tilt.Equals(FQuat::Identity, 1.e-6))
	{
		return RawAnchorTransform;
	}

	// The raw actor carries the tilt GetAnchorTiltApplications() times, the HMD and controllers once: remove the rest.
	const FTransform TrackingToWorld = GetTrackingToWorld();
	const FQuat Correction = TiltPower(Tilt, 1 - GetAnchorTiltApplications());

	FTransform Out = RawAnchorTransform;
	Out.SetLocation(RotatePositionInTrackingSpace(RawAnchorTransform.GetLocation(), Correction, TrackingToWorld));
	Out.SetRotation(RotateOrientationInTrackingSpace(RawAnchorTransform.GetRotation(), Correction, TrackingToWorld));
	return Out;
}

bool UAnchorsManagerSubsystem::HasValidAnchorsFrame() const
{
	FTransform Frame;
	return TryGetAnchorsFrame(Frame);
}

bool UAnchorsManagerSubsystem::GetAnchorsFrameTransform(FTransform& OutFrame) const
{
	return TryGetAnchorsFrame(OutFrame);
}

FTransform UAnchorsManagerSubsystem::GetRelativeToAnchorsFrame(const FTransform& WorldTransform) const
{
	FTransform Frame;
	if (!TryGetAnchorsFrame(Frame))
	{
		UE_LOG(LogTemp, Warning, TEXT("[AnchorsManager] GetRelativeToAnchorsFrame: anchors not ready, returning input unchanged."));
		return WorldTransform;
	}
	
	const FVector LocalPos = Frame.InverseTransformPosition(WorldTransform.GetLocation());
	const FQuat   LocalRot = Frame.GetRotation().Inverse() * WorldTransform.GetRotation();

	return FTransform(LocalRot, LocalPos, WorldTransform.GetScale3D());
}

FTransform UAnchorsManagerSubsystem::GetWorldFromAnchorsFrame(const FTransform& LocalTransform) const
{
	FTransform Frame;
	if (!TryGetAnchorsFrame(Frame))
	{
		UE_LOG(LogTemp, Warning, TEXT("[AnchorsManager] GetWorldFromAnchorsFrame: anchors not ready, returning input unchanged."));
		return LocalTransform;
	}

	const FVector WorldPos = Frame.TransformPosition(LocalTransform.GetLocation());
	const FQuat   WorldRot = Frame.GetRotation() * LocalTransform.GetRotation();

	return FTransform(WorldRot, WorldPos, LocalTransform.GetScale3D());
}
