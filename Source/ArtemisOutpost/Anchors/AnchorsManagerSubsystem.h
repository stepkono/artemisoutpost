// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CesiumGeoreference.h"
#include "OculusXRAnchors.h"
#include "OculusXRAnchorsRequests.h"
#include "ArtemisOutpost/GameData/ArtemisGameInstance.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AnchorsManagerSubsystem.generated.h"

/** Broadcast when ShareAnchorsWithGroup finishes — true on success, false on failure. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAnchorsSharedResult, bool, bSuccess);

/**
 * Service subsystem responsible for spatial anchor discovery, sharing, and spawning.
 * Lives on GameInstance — survives level loads.
 */
UCLASS(BlueprintType)
class ARTEMISOUTPOST_API UAnchorsManagerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/**
	 * Discovers local anchors by UUID, spawns a hidden actor for each.
	 * Results arrive in A(0) B(1) C(2) D(3) order via OnComplete.
	 */
	void DiscoverAnchors(const FOrderedAnchors& RawAnchors, TFunction<void(TArray<AActor*>&)> OnComplete);

	/**
	 * Saves the given anchor actors and shares them with the hardcoded group.
	 * The actors must carry a UOculusXRAnchorComponent with a valid handle
	 * (i.e., they were spawned via SpawnActorWithAnchorHandle).
	 * Fires OnAnchorsSharedResult when the operation completes.
	 *
	 * @param AnchorActors  The spawned anchor actors to share.
	 */
	UFUNCTION(BlueprintCallable, Category="Spatial Anchors")
	void ShareAnchorsWithGroup(const TArray<AActor*>& AnchorActors);

	/**
	 * Retrieves anchors shared with the hardcoded group and spawns actors in A/B/C/D order.
	 * Uses the recommended GetSharedAnchorsAsync (group-based) API.
	 *
	 * @param RawAnchors  The expected anchor UUIDs (for A/B/C/D ordering).
	 * @param OnComplete  Callback with spawned actors in A(0) B(1) C(2) D(3) order.
	 */
	void RequestSharedAnchors(const FOrderedAnchors& RawAnchors, TFunction<void(TArray<AActor*>&)> OnComplete);

	UFUNCTION(BlueprintCallable, Category="Spatial Anchors")
	AActor* GetBaseAnchor();

	/** Fired when ShareAnchorsWithGroup completes. */
	UPROPERTY(BlueprintAssignable, Category="Spatial Anchors")
	FOnAnchorsSharedResult OnAnchorsSharedResult;
	
	UFUNCTION(BlueprintCallable, Category="Spatial Anchors")
	TArray<AActor*> GetAnchors(); 

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Spatial Anchors")
	FTransform GetRelativeToAnchorsFrame(const FTransform& WorldTransform) const;
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Spatial Anchors")
	FTransform GetWorldFromAnchorsFrame(const FTransform& LocalTransform) const;

	/** True when the anchors frame can be built, i.e. the frame conversions return meaningful values. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Spatial Anchors")
	bool HasValidAnchorsFrame() const;

	/**
	 * The shared table frame: rotation oriented by the table edges, translation at the table center, uniform
	 * scale = table edge length in UE units (a mesh of size 1 on X spans one table edge). False when not ready.
	 */
	UFUNCTION(BlueprintCallable, Category="Spatial Anchors")
	bool GetAnchorsFrameTransform(FTransform& OutFrame) const;

	/**
	 * Converts a pose read from a raw anchor ACTOR into the space the HMD and controllers live in. In VR the actors carry
	 * the base-orientation tilt artemis.AnchorTiltApplications times (MetaXR bug: 2), the tracked devices once, so the
	 * difference is removed (in tracking space). Identity in AR. Not needed for anything from the anchors frame.
	 */
	UFUNCTION(BlueprintPure, Category="Spatial Anchors")
	static FTransform RawAnchorToTrackedSpace(const FTransform& RawAnchorTransform);

private:
	/**
	 * Builds the shared table frame as a similarity transform: orthonormal basis oriented by the
	 * table edges, translation at the table center, uniform scale = table edge length. Returns
	 * false when the anchors are not spawned/located yet.
	 */
	bool TryGetAnchorsFrame(FTransform& OutFrame) const;

	UFUNCTION()
	void OnAnchorDiscovered(const TArray<FOculusXRAnchorsDiscoverResult>& DiscoveredAnchors);

	UFUNCTION()
	void OnDiscoveryComplete(EOculusXRAnchorResult::Type Result);

	/** Broadcasts OnAnchorsSharedResult exactly once per share attempt and stops the timeout. */
	void ConcludeShare(bool bSuccess);

	/** Fires if the share flow never completes (e.g. a dropped SDK callback). */
	void OnShareTimeout();

	void SpawnRawAnchors(const TArray<FOculusXRAnchorsDiscoverResult>& RawOrderedAnchorsToSpawn);

	void WaitForAnchorsLocated(const int32 CallCount);

	void RemoveOldAnchors();

	/** Logs common OculusXR anchor error codes with human-readable context. */
	static void LogAnchorError(const TCHAR* Context, EOculusXRAnchorResult::Type Result);

private:
	FOculusXRDiscoverAnchorsResultsDelegate DiscoveredAnchorDelegate;
	FOculusXRDiscoverAnchorsCompleteDelegate DiscoveredAnchorsCompleteDelegate;

	/** Ordered list of UUIDs (A=0, B=1, C=2, D=3) — used to restore index after unordered results. */
	TArray<FOculusXRUUID> OrderedUUIDs;

	/** Guards OnAnchorsSharedResult so it broadcasts at most once per share attempt. */
	bool bShareConcluded = false;

	/** Number of anchors the current share attempt is waiting on (for timeout diagnostics). */
	int32 PendingShareCount = 0;

	/** Timer that aborts the share flow if the SDK never reports completion. */
	FTimerHandle ShareTimeoutTimer;

	/** Seconds to wait for the share flow before giving up. */
	static constexpr float ShareTimeoutSec = 30.0f;

	/** Accumulates raw discovery results across multiple OnAnchorDiscovered callbacks. */
	TArray<FOculusXRAnchorsDiscoverResult> UnorderedDiscoveredAnchors;

	/** Fired in OnDiscoveryComplete / SpawnRawAnchors with spawned actors in A/B/C/D order. */
	TFunction<void(TArray<AActor*>&)> PendingCallback;

	/** Timer handle for the IsLocated() polling loop. */
	FTimerHandle LocatedPollTimer;

	/** How many poll ticks have fired since WaitForAnchorsLocated was started. */
	int32 LocatedPollAttempts = 0;

	/** Seconds between each IsLocated() poll. */
	static constexpr float LocatedPollIntervalSec = 0.05f;

	/** Maximum poll attempts before giving up (~3 seconds at 50 ms intervals). */
	static constexpr int32 LocatedPollMaxAttempts = 60;

	UPROPERTY()
	ACesiumGeoreference* Moon;

	UPROPERTY()
	AActor* BaseAnchor;

	UPROPERTY()
	TSubclassOf<AActor> AnchorClass;

	UPROPERTY()
	TArray<AActor*> SpawnedAnchors;
	
	UPROPERTY()
	int32 CallsCountToDiscover = 0; 
	
	UPROPERTY()
	int32 CallsCountToSpawn = 0; 
	
	/** Fixed group UUID for anchor sharing — all devices use the same group. */
	FOculusXRUUID SharingGroupUUID;
};
