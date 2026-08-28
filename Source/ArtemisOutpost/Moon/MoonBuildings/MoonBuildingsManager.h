// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ArtemisOutpost/MiniGames/General/MinigameTypes.h"
#include "MoonBuildingsManager.generated.h"

// Fired (server-side) whenever a minigame/building registers. Signal Towers without a target listen
// so they can claim a habitat that is built after them.
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMinigameRegistered, const FMiniGameRecord& /*Record*/);

// Server-authoritative registry of every placed minigame/building on the moon, keyed on MGID. Also
// owns the Signal-Tower -> Habitat claim logic. Server-only in practice (all mutating calls are made
// from HasAuthority paths); the result (target angle) reaches clients via the replicated minigame
// actor, so this subsystem itself never replicates.
UCLASS()
class ARTEMISOUTPOST_API UMoonBuildingsManager : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// Add a building to the registry and notify listeners. Call once per minigame, server-side.
	void RegisterMinigame(const FMiniGameRecord& Record);

	// Update the cached lifecycle state of a registered building.
	void UpdateState(const FGuid& MGID, EMinigameState NewState);

	// Finds the nearest un-activated, un-assigned habitat within Radius of TowerLocation, ATOMICALLY
	// marks it assigned to TowerMGID (permanent claim), and returns it. Single-threaded game logic
	// means no real race: the claim is committed before this returns, so a second tower sees it taken.
	// Returns false if no habitat qualifies.
	bool TryClaimHabitatFor(const FGuid& TowerMGID, const FVector& TowerLocation, float Radius,
		FGuid& OutHabitatMGID, FVector& OutHabitatLocation);

	FOnMinigameRegistered OnMinigameRegistered;

private:
	UPROPERTY()
	TMap<FGuid, FMiniGameRecord> Buildings;
};
