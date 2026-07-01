// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "ArtemisOutpost/Cesium/GeoRefsManager.h"
#include "ArtemisOutpost/GameData/ArtemisGameState.h"
#include "AMasterRover.generated.h"

class APuppetRover;

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

	// On clients, records the server's authoritative transform as the target that Tick eases the
	// (non-simulating) master toward. See AMasterRover.cpp for why we don't apply it directly here.
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

	// Latest replicated transform from the server, used as the target the client master eases toward
	// (recorded in OnRep_ReplicatedMovement; consumed by Tick's smoothing).
	FVector LastRepMoveLocation = FVector::ZeroVector;
	FRotator LastRepMoveRotation = FRotator::ZeroRotator;
	bool bHasRepTarget = false;

	// False until the client master has been placed once, so the first update snaps (no slide-in from spawn).
	bool bClientSmoothingInit = false;
};
