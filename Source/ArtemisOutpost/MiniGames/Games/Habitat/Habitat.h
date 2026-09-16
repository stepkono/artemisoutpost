// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArtemisOutpost/MiniGames/General/GameInstance/CoupledAxisMinigameActor.h"
#include "ArtemisOutpost/MiniGames/Games/Habitat/HabitatTypes.h"
#include "Habitat.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHabitatPhaseChangedBP, EHabitatPhase, OldPhase, EHabitatPhase, NewPhase);

// The habitat foundation (§8.6). Two coupled levelling axes: [0] = Pitch, [1] = Roll, both with a
// target of 0 degrees (level). The foundation spawns with a random tilt on each axis, so it is never
// level at placement.
//
// What makes it different from the Signal Tower is that it is GENUINELY cooperative:
//  - Rotation is only applied while BOTH players are on their rotation screen, i.e. while both axes
//    are owned (Phase == Leveling). Until then the axes do not move, dwell stays at zero, and no
//    input is queued, so nothing applies late once the partner arrives.
//  - The task completes from the server tick once every axis has been within tolerance for
//    DwellSeconds SIMULTANEOUSLY. Drifting out resets that axis' timer; a partner leaving closes the
//    gate and zeroes every timer. Leaving never commits anything (SolvesAxisOnLeave = false).
//  - Axis values are frozen, not reset, when a player leaves: they model the physical tilt of the
//    foundation, and walking away does not un-tilt it.
//
// Meshes, the connection prompt and the world-space visuals live on BP_Habitat. Registers with
// FHabitatData so a Signal Tower can claim it once it is levelled (State == Completed).
UCLASS()
class ARTEMISOUTPOST_API AHabitat : public ACoupledAxisMinigameActor
{
	GENERATED_BODY()

public:
	static constexpr int32 AxisPitch = 0;
	static constexpr int32 AxisRoll = 1;

	AHabitat();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// The replicated, derived sub-state of Active. See EHabitatPhase.
	UFUNCTION(BlueprintPure, Category = "Habitat")
	EHabitatPhase GetPhase() const { return Phase; }

	// Signed tilt (-180..180) of an axis, for meshes and the shared level bubble. The replicated
	// FAxisData::Value is 0..360; 0 is level, so a small negative tilt sits just below 360.
	UFUNCTION(BlueprintPure, Category = "Habitat")
	float GetSignedTiltDeg(int32 AxisIndex) const;

	// Fires on every peer when the phase changed. BP_Habitat drives the "waiting" / "levelling" cue.
	UPROPERTY(BlueprintAssignable, Category = "Habitat")
	FOnHabitatPhaseChangedBP OnPhaseChanged;

	// Rotation is open only while both axes are owned. OutReason names the missing condition.
	virtual bool IsRotationOpen(FString& OutReason) const override;

protected:
	virtual void BeginPlay() override;

	// Coupled-axis rules.
	virtual int32 GetAxisCount() const override;
	virtual float GetAxisTarget(int32 AxisIndex) const override;
	virtual bool SolvesAxisOnLeave() const override;
	virtual void EvaluateCompletion() override;
	virtual void OnAxisOwnershipChanged() override;

	// Lifecycle.
	virtual void OnStateChangedNative(EMinigameState NewState) override;
	virtual void OnComplete() override;
	virtual void SyncPuppet() override;
	virtual TSharedRef<class FJsonObject> BuildSnapshot() const override;

	// Registry.
	virtual EMiniGameType GetBuildingType() const override { return EMiniGameType::Habitat; }
	virtual FInstancedStruct MakeInitialTypeData() const override;

	// Fired server-side when the foundation was levelled (both axes dwelled in tolerance together).
	// BP hooks VFX / sound. The registry state flips to Completed in the same step, which is what makes
	// this habitat claimable by a Signal Tower.
	UFUNCTION(BlueprintImplementableEvent, Category = "Habitat")
	void OnHabitatLevelled();

	// ---- Designer tuning ----

	// Magnitude range (deg) of the random tilt each axis starts with. Both axes get an independent
	// magnitude in [Min, Max] and an independent sign, so the foundation is never level at spawn and
	// never predictable.
	UPROPERTY(EditAnywhere, Category = "Habitat", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float InitialTiltMinDeg = 15.0f;

	UPROPERTY(EditAnywhere, Category = "Habitat", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float InitialTiltMaxDeg = 45.0f;

private:
	// Server: recompute Phase from State + ownership (HabitatRules::DerivePhase), log the transition,
	// notify BP and the puppet. Idempotent.
	void RefreshPhase();

	// Local reaction to a phase change (server directly, clients via OnRep): BP delegate + puppet.
	void HandlePhaseChanged(EHabitatPhase OldPhase);

	void PushPhaseToPuppet();

	UFUNCTION()
	void OnRep_Phase(EHabitatPhase OldPhase);

	UPROPERTY(ReplicatedUsing = OnRep_Phase)
	EHabitatPhase Phase = EHabitatPhase::Inactive;
};
