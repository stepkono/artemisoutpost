// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArtemisOutpost/MiniGames/General/GameInstance/CoupledAxisMinigameActor.h"
#include "SignalTower.generated.h"

// The radio mast (Funkmast). Two coupled alignment axes — [0] toward Earth, [1] toward a Habitat
// (§8.7). Earth is a random bearing every time (never memorizable). Habitat would derive from the
// chosen habitat's geodetic bearing, but there is no habitat data model yet, so it is a random
// placeholder for now. Meshes, the world-space prompt and the AR/VR beams live on BP_SignalTower.
UCLASS()
class ARTEMISOUTPOST_API ASignalTower : public ACoupledAxisMinigameActor
{
	GENERATED_BODY()

public:
	static constexpr int32 AxisEarth = 0;
	static constexpr int32 AxisHabitat = 1;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual int32 GetAxisCount() const override;
	virtual float GetAxisTarget(int32 AxisIndex) const override;
	virtual bool CanStart(const FString& UPID, FText& OutReason) const override;
	virtual void OnComplete() override;
	virtual void SyncPuppet() override;
	virtual EOutpostBuildingType GetBuildingType() const override { return EOutpostBuildingType::SignalTower; }

	// Fired server-side when both axes are aligned. BP hooks habitat activation, score,
	// influence-radius growth, VFX.
	UFUNCTION(BlueprintImplementableEvent, Category = "Signal Tower")
	void OnTowerActivated();

	// Max distance (cm) to a habitat this tower may point at. Editable per BP/instance.
	UPROPERTY(EditDefaultsOnly, Category = "Signal Tower")
	float SignalRadius = 5000.0f;

private:
	// Server: try to claim the nearest free habitat in range and derive the habitat target bearing.
	// Sets bHasTarget + HabitatTargetDeg on success. Called at BeginPlay and when a new habitat
	// registers (in case the habitat is built after this tower).
	void TryClaimTargetHabitat();

	// Server: reaction to a newly registered building (bound only while we have no target yet).
	void HandleMinigameRegistered(const FMiniGameRecord& Record);

	// Bearing (deg, 0 = local forward, CW around up) from this tower to a world location, projected
	// onto the surface tangent plane.
	float BearingToDeg(const FVector& WorldLocation) const;

	// Pushes the (one-time) habitat target to the AR puppet via the generic ApplyData channel.
	void PushHabitatTargetToPuppet();

	UFUNCTION()
	void OnRep_HabitatTarget();

	// Server-only bookkeeping.
	bool bHasTarget = false;
	FGuid TargetHabitatMGID;

	// Fixed target bearings (deg), set once server-side, replicated for client display. Earth is
	// arbitrary; Habitat points at the claimed habitat. Targets deliberately live here, NOT in the
	// per-rotation FAxisData stream.
	UPROPERTY(Replicated)
	float EarthTargetDeg = 0.0f;

	UPROPERTY(ReplicatedUsing = OnRep_HabitatTarget)
	float HabitatTargetDeg = 0.0f;
};
