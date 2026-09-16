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

// Fired (server-side) whenever a registered building's lifecycle state changes. Lean on purpose (no
// provider payload): the one listener today is a Signal Tower without a target, which retries its claim
// when a Habitat reaches Completed, because only a LEVELLED habitat is claimable.
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnMinigameStateChanged, const FGuid& /*MGID*/, EMiniGameType, EMinigameState);

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

	// Update the cached lifecycle state of a registered building and notify OnMinigameStateChanged.
	void UpdateState(const FGuid& MGID, EMinigameState NewState);

	// Finds the nearest LEVELLED (State == Completed), un-activated, un-assigned habitat within Radius
	// of TowerUELocation, ATOMICALLY marks it assigned to TowerMGID (permanent claim), and returns it.
	// Single-threaded game logic means no real race: the claim is committed before this returns, so a
	// second tower sees it taken. Returns false if no habitat qualifies.
	//
	// The Completed requirement is the scenario order (§8.6 -> §8.7): the foundation is levelled by two
	// people first, only then can a tower point at it. A habitat that is still Idle or being played is
	// skipped with a logged reason; the tower retries via OnMinigameStateChanged once it completes.
	//
	// COORDINATE SPACES: records store geodetic positions (BuildGeoLocation), because that is what
	// travels over the network and stays valid regardless of a peer's georeference origin. The tower
	// asks in UE world space, so each candidate is converted geo -> UE here before the range test.
	// Radius and OutHabitatUELocation are therefore both UE world space (cm).
	bool TryClaimHabitatFor(const FGuid& TowerMGID, const FVector& TowerUELocation, float Radius, FGuid& OutHabitatMGID, FVector& OutHabitatUELocation);

	// Marks a habitat as activated (the tower pointing at it was aligned). Permanent: an activated
	// habitat is never claimable again and counts for the score. Logs and returns false when the MGID is
	// unknown, not a habitat, or carries no FHabitatData.
	bool MarkHabitatActivated(const FGuid& HabitatMGID, const FGuid& ByTowerMGID);

	FOnMinigameRegistered OnMinigameRegistered;
	FOnMinigameStateChanged OnMinigameStateChanged;

private:
	// Lazily resolved: this subsystem is created before the level's actors, so the georeference
	// cannot be looked up at initialization time. Re-resolves while still null.
	AGeoRefsManager* GetGeoRefsManager();

	UPROPERTY()
	TMap<FGuid, FMiniGameRecord> Buildings;

	UPROPERTY(Transient)
	AGeoRefsManager* CachedGeoRefsManager = nullptr;
};
