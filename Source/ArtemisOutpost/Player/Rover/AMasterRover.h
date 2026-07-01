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

	// Client master motion smoothing. The client master doesn't simulate; it receives the server's
	// transform at network cadence, which is choppy. We ease the MASTER toward the latest replicated
	// target each frame (VInterpTo/QInterpTo) so it moves smoothly on the VR moon — visible when a
	// user is in VR — and the puppet, which copies the master, is smooth for free. Higher speed =
	// snappier/less smooth; lower = smoother/laggier. Tunable in the BP defaults.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rover Smoothing")
	float ClientLocationInterpSpeed = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rover Smoothing")
	float ClientRotationInterpSpeed = 12.0f;

	// If the target jumps farther than this (VR-moon world units) we snap instead of easing, so a
	// teleport / respawn / big correction doesn't produce a slow slide across the moon.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rover Smoothing")
	float ClientSnapDistance = 5000.0f;
	
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
	FVector LastRepMoveLocation = FVector::ZeroVector;    // latest replicated target location (VR-moon world)
	FRotator LastRepMoveRotation = FRotator::ZeroRotator; // latest replicated target rotation
	bool bHasRepTarget = false;                            // a replicated transform has arrived at least once

	// False until the client master has been placed once, so the first update snaps (no slide-in from spawn).
	bool bClientSmoothingInit = false;

	// Client-side VR-moon tileset (the moon the master physically drives on), resolved by the
	// "DEFAULT_TILESET" tag in BeginPlay. Used to report client streaming/culling state.
	UPROPERTY()
	ACesium3DTileset* ClientVRTileset = nullptr;
};
