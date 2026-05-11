// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OculusXRAnchors.h"
#include "GameData/ArtemisGameInstance.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AnchorsManagerSubsystem.generated.h"

/**
 * Service subsystem responsible for spatial anchor discovery and spawning.
 * Lives on GameInstance — survives level loads.
 * Does not subscribe to events itself; call DiscoverAnchors() directly.
 */
UCLASS()
class ARTEMISOUTPOST_API UAnchorsManagerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/**
	 * Discovers anchors by UUID, spawns an actor for each using the class set in
	 * Project Settings -> Artemis Anchor Settings -> SpatialAnchorModelClass.
	 * Calls OnComplete with the spawned actors when all anchors have been located.
	 */
	void DiscoverAnchors(const FCustomAnchors& RawAnchors, TFunction<void(TArray<AActor*>)> OnComplete);

private:
	UFUNCTION()
	void OnAnchorDiscovered(const TArray<FOculusXRAnchorsDiscoverResult>& DiscoveredAnchors);

	UFUNCTION()
	void OnDiscoveryComplete(EOculusXRAnchorResult::Type Result);

private:
	FOculusXRDiscoverAnchorsResultsDelegate DiscoveredAnchorDelegate;
	FOculusXRDiscoverAnchorsCompleteDelegate DiscoveredAnchorsCompleteDelegate;

	/** Ordered list of UUIDs (A=0, B=1, C=2, D=3) — used to restore index after unordered discovery results. */
	TArray<FOculusXRUUID> OrderedUUIDs;

	/** Accumulates raw discovery results across multiple OnAnchorDiscovered callbacks. */
	TArray<FOculusXRAnchorsDiscoverResult> AnchorsToSpawn;

	/** Fired in OnDiscoveryComplete with spawned actors in A/B/C/D order. */
	TFunction<void(TArray<AActor*>)> PendingCallback;
};
