// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "ArtemisOutpost/Cesium/GeoRefsManager.h"
#include "ArtemisOutpost/GameData/ArtemisGameState.h"
#include "AMasterRover.generated.h"

class APuppetRover;
class ACesium3DTileset;

/**
 * 
 */
UCLASS()
class ARTEMISOUTPOST_API AMasterRover : public AWheeledVehiclePawn
{
	GENERATED_BODY()
	
public:
	AMasterRover();
	
	virtual void BeginPlay() override;
	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
	UFUNCTION()
	FVector GetLocalPos_UE() const; 
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// [BasketA] Fires on clients when the server's authoritative transform correction lands.
	// We timestamp it so Tick can report how stale the last correction is (corrections stalling
	// is the suspected trigger for the client-local sim drifting/falling through).
	virtual void OnRep_ReplicatedMovement() override;
	
	UFUNCTION(BlueprintCallable, Category = "Puppet Rover")
	APuppetRover* GetPuppetRover(); 
	
	UFUNCTION(BlueprintCallable, Category = "GeoRefsManager")
	void SetGeoRefsManager(AGeoRefsManager* InManager); 
	
private: 
	UFUNCTION()
	void HandleControls(FControlCommand& ControlCommand); 
	
protected:
	UPROPERTY(BlueprintReadOnly, Category="GeoRefsManager")
	AGeoRefsManager* GeoRefsManager;
	
	UPROPERTY(BlueprintReadWrite, Category="Puppet Rover")
	APuppetRover* PuppetRover;
	
	UPROPERTY(BlueprintReadOnly, Category="Game State")
	AArtemisGameState* GameState;
	
private:
	UPROPERTY()
	FVector StartLocalPosition_UE;

	// Accumulates DeltaTime so Tick can log roughly every LogIntervalSeconds.
	float LogTimeAccumulator = 0.0f;
	static constexpr float LogIntervalSeconds = 5.0f;

	// [Collision] Separate, faster (~1 Hz) accumulator for the server-side collision-resolution
	// probe — sink/hover is transient, so the 5 s cadence above would miss it.
	float CollisionLogAccumulator = 0.0f;

	// Per-tick position tracking to quantify the jitter/bounce without per-frame log spam:
	// every tick we measure |dPos| and keep the PEAK; the throttled log reports that peak.
	FVector LastTickPos = FVector::ZeroVector;
	bool bHasLastTickPos = false;
	float MaxTickDelta = 0.0f;

	// ---- [BasketA] client streaming / replication-divergence diagnostics ----
	// Timestamp + location of the last replicated movement correction received from the server.
	// If the local (client-simulated) transform drifts far from this while the age grows, the
	// client is free-simulating away from server truth (the suspected fall-through cause).
	double LastRepMoveWorldTime = -1.0;
	int32 RepMoveUpdateCount = 0;
	FVector LastRepMoveLocation = FVector::ZeroVector;

	// Client-side VR-moon tileset (the moon the master physically drives on), resolved by the
	// "DEFAULT_TILESET" tag in BeginPlay. Used to report client streaming/culling state.
	UPROPERTY()
	ACesium3DTileset* ClientVRTileset = nullptr;
};
