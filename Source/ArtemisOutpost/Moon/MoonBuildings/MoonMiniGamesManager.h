// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ArtemisOutpost/MiniGames/General/MinigameTypes.h"
#include "ArtemisOutpost/StudyData/Providers/DataProviderSubsystemBase.h"
#include "ArtemisOutpost/StudyData/Types/MiniGameType/MiniGameProviderData.h"
#include "MoonMiniGamesManager.generated.h"

class AGeoRefsManager;

// Fired (server-side) whenever a minigame/building registers. Signal Towers without a target listen
// so they can claim a habitat that is built after them.
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnMinigameRegistered, UProviderDataBase&, const EGameEventType);

// Server-authoritative registry of every placed minigame/building on the moon, keyed on MGID. Also
// owns the Signal-Tower -> Habitat claim logic. Server-only in practice (all mutating calls are made
// from HasAuthority paths); the result (target angle) reaches clients via the replicated minigame
// actor, so this subsystem itself never replicates.
UCLASS()
class ARTEMISOUTPOST_API UMoonMiniGamesManager : public UDataProviderSubsystemBase
{
	GENERATED_BODY()

public:
	// Add a building to the registry and notify listeners. Call once per minigame, server-side.
	void RegisterMinigame(const FMiniGameRecord& Record);

	// Update the cached lifecycle state of a registered building.
	void UpdateState(const FGuid& MGID, EMinigameState NewState);

	// Finds the nearest un-activated, un-assigned habitat within Radius of TowerUELocation, ATOMICALLY
	// marks it assigned to TowerMGID (permanent claim), and returns it. Single-threaded game logic
	// means no real race: the claim is committed before this returns, so a second tower sees it taken.
	// Returns false if no habitat qualifies.
	//
	// COORDINATE SPACES: records store geodetic positions (BuildGeoLocation), because that is what
	// travels over the network and stays valid regardless of a peer's georeference origin. The tower
	// asks in UE world space, so each candidate is converted geo -> UE here before the range test.
	// Radius and OutHabitatUELocation are therefore both UE world space (cm).
	bool TryClaimHabitatFor(const FGuid& TowerMGID, const FVector& TowerUELocation, float Radius, FGuid& OutHabitatMGID, FVector& OutHabitatUELocation);

	FOnMinigameRegistered OnMinigameRegistered;

private:
	// Lazily resolved: this subsystem is created before the level's actors, so the georeference
	// cannot be looked up at initialization time. Re-resolves while still null.
	AGeoRefsManager* GetGeoRefsManager();

	UPROPERTY()
	TMap<FGuid, FMiniGameRecord> Buildings;

	UPROPERTY(Transient)
	AGeoRefsManager* CachedGeoRefsManager = nullptr;
};
