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
UCLASS()
class ARTEMISOUTPOST_API UAnchorsManagerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/**
	 * Discovers local anchors by UUID, spawns a hidden actor for each.
	 * Results arrive in A(0) B(1) C(2) D(3) order via OnComplete.
	 */
	void DiscoverAnchors(const FCustomAnchors& RawAnchors, TFunction<void(TArray<AActor*>)> OnComplete);

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
	void RequestSharedAnchors(const FCustomAnchors& RawAnchors, TFunction<void(TArray<AActor*>)> OnComplete);

	UFUNCTION(BlueprintCallable, Category="Spatial Anchors")
	AActor* GetBaseAnchor();

	/** Fired when ShareAnchorsWithGroup completes. */
	UPROPERTY(BlueprintAssignable, Category="Spatial Anchors")
	FOnAnchorsSharedResult OnAnchorsSharedResult;

private:
	UFUNCTION()
	void OnAnchorDiscovered(const TArray<FOculusXRAnchorsDiscoverResult>& DiscoveredAnchors);

	UFUNCTION()
	void OnDiscoveryComplete(EOculusXRAnchorResult::Type Result);

	void SpawnRawAnchors(const TArray<FOculusXRAnchorsDiscoverResult>& RawAnchorsToSpawn);

	/** Logs common OculusXR anchor error codes with human-readable context. */
	static void LogAnchorError(const TCHAR* Context, EOculusXRAnchorResult::Type Result);

private:
	FOculusXRDiscoverAnchorsResultsDelegate DiscoveredAnchorDelegate;
	FOculusXRDiscoverAnchorsCompleteDelegate DiscoveredAnchorsCompleteDelegate;

	/** Ordered list of UUIDs (A=0, B=1, C=2, D=3) — used to restore index after unordered results. */
	TArray<FOculusXRUUID> OrderedUUIDs;

	/** Accumulates raw discovery results across multiple OnAnchorDiscovered callbacks. */
	TArray<FOculusXRAnchorsDiscoverResult> UnorderedDiscoveredAnchors;

	/** Fired in OnDiscoveryComplete / SpawnRawAnchors with spawned actors in A/B/C/D order. */
	TFunction<void(TArray<AActor*>)> PendingCallback;

	UPROPERTY()
	ACesiumGeoreference* Moon;

	UPROPERTY()
	AActor* BaseAnchor;

	UPROPERTY()
	TSubclassOf<AActor> AnchorClass;

	/** Fixed group UUID for anchor sharing — all devices use the same group. */
	FOculusXRUUID SharingGroupUUID;
};
