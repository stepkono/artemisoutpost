// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArtemisOutpost/MiniGames/General/GameInstance/MinigameActor.h"
#include "ArtemisOutpost/MiniGames/General/MinigameTypes.h"
#include "CoupledAxisMinigameActor.generated.h"

// Broadcast on every peer whenever the axes change (value, target, or ownership). Subscribers: the
// per-player controller (forwards to the screen UI) and the actor BP (meshes/beams). Fires on the
// server directly from the mechanics and on remote clients from OnRep.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAxesUpdated, const TArray<FAxisData>&, Axes);

// Reusable "two people, two locked degrees of freedom" minigame actor. Signal Tower (Earth +
// Habitat) and Habitat (Pitch + Roll) both derive from this. It OWNS the replicated Axes, runs the
// mechanics (claim/release/rotate + dwell/completion) and holds the coupled-axis rules itself as
// config properties + overridable GetAxisCount/InitAxisTargets — no separate rules component.
//
// Ownership is EXPLICIT: a participant claims an axis (the selection screen), owns exactly one at a
// time, and only the owner may rotate it. A second participant can only claim a still-free axis, so
// the two are coupled one-per-person (§8.6/§8.7). Solo: claim one, align it, claim the other (the
// first keeps its value) — completion checks all axes regardless of owner.
UCLASS(Abstract)
class ARTEMISOUTPOST_API ACoupledAxisMinigameActor : public AMinigameActor
{
	GENERATED_BODY()

public:
	ACoupledAxisMinigameActor();
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(BlueprintAssignable, Category = "Minigame")
	FOnAxesUpdated OnAxesUpdated;

	UFUNCTION(BlueprintPure, Category = "Minigame")
	const TArray<FAxisData>& GetAxes() const;

	UFUNCTION(BlueprintPure, Category = "Minigame")
	int32 GetNumAxes() const;

	// 0..1 dwell fraction of an axis, for UI / awareness cues.
	UFUNCTION(BlueprintPure, Category = "Minigame")
	float GetAxisProgress(int32 AxisIndex) const;

	// Whether UPID owns (may rotate) this axis.
	UFUNCTION(BlueprintPure, Category = "Minigame")
	bool CanControlAxis(const FString& UPID, int32 AxisIndex) const;

	// Whether an axis is currently within tolerance (green "levelled" feedback).
	UFUNCTION(BlueprintPure, Category = "Minigame")
	bool IsAxisAligned(int32 AxisIndex) const;

	// Dwell duration, pushed to the screen UI to draw a progress ring from InToleranceTime.
	UFUNCTION(BlueprintPure, Category = "Minigame")
	float GetDwellSeconds() const { return DwellSeconds; }

protected:
	virtual void OnStart() override;
	virtual void OnAbort() override;
	virtual void ApplyInput(const FString& UPID, const FMinigameInput& Input) override;
	virtual void OnParticipantLeft(const FString& UPID) override;
	virtual void SyncPuppet() override;
	virtual TSharedRef<class FJsonObject> BuildSnapshot() const override;

	// --- Coupled-axis rules (overridable per game) ---

	// Number of controllable degrees of freedom.
	virtual int32 GetAxisCount() const;

	// Fills TargetValue for each axis (server-side, on start). Axes is already sized to GetAxisCount.
	virtual void InitAxisTargets(TArray<FAxisData>& InAxes) const;

	// --- Designer tuning (§10) ---

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minigame")
	float AxisToleranceDeg = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minigame")
	float DwellSeconds = 2.5f;

	// Anti-teleport clamp: the largest angle (degrees) a single input message may add.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minigame")
	float MaxStepPerInputDeg = 45.0f;

	UPROPERTY(ReplicatedUsing = OnRep_UpdateAxes, BlueprintReadOnly, Category = "Minigame")
	TArray<FAxisData> Axes;

	UFUNCTION()
	void OnRep_UpdateAxes();

private:
	// Broadcasts OnAxesUpdated to local subscribers (screen UI) AND pushes the axes to the AR puppet.
	// Single funnel so the listen host (server-authored changes, no OnRep) and remote clients (OnRep)
	// both keep UI + puppet in sync.
	void NotifyAxesUpdated();

	// Mechanics (server).
	void ClaimAxis(const FString& UPID, int32 AxisIndex);
	void ReleaseAxis(const FString& UPID, int32 AxisIndex);
	void ReleaseAxesOf(const FString& UPID);
	void RotateAxis(const FString& UPID, int32 AxisIndex, float DeltaDegrees);
	void EvaluateCompletion(float DeltaTime);

	static float NormalizeDeg(float Angle);
	static float AngularDistanceDeg(float A, float B);
};
