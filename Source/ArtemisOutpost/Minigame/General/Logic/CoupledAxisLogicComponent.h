// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MinigameLogicComponent.h"
#include "CoupledAxisLogicComponent.generated.h"

// Broadcast on every peer whenever the axes change (value, target, or ownership). Subscribers:
// the owning actor's BP (mesh/beams) and the per-player controller (forwards to the screen UI).
// Fires on the server directly from OnStart/ApplyInput/OnAbort, on remote clients from OnRep.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAxesUpdated, const TArray<FAxisState>&, Axes);

// Reusable "two people, two locked degrees of freedom" rules. Signal Tower (Earth + Habitat)
// and Habitat (Pitch + Roll) both derive from this.
//
// Ownership is EXPLICIT: a participant claims an axis (the selection screen), owns exactly one
// at a time, and only the owner may rotate it. A second participant can only claim a still-free
// axis, so the two are coupled one-per-person (§8.6/§8.7). Solo: claim one, align it, claim the
// other (the first keeps its value) — completion checks all axes regardless of owner.
UCLASS(Abstract, ClassGroup = (Minigame))
class ARTEMISOUTPOST_API UCoupledAxisLogicComponent : public UMinigameLogicComponent
{
	GENERATED_BODY()

public:
	UCoupledAxisLogicComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(BlueprintAssignable, Category = "Minigame")
	FOnAxesUpdated OnAxesUpdated;

	UFUNCTION(BlueprintPure, Category = "Minigame")
	const TArray<FAxisState>& GetAxes() const;

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

	// Dwell duration; pushed to the screen UI so it can draw a progress ring from InToleranceTime.
	UFUNCTION(BlueprintPure, Category = "Minigame")
	float GetDwellSeconds() const;

protected:
	virtual void OnStart() override;
	virtual void OnParticipantJoined(const FString& UPID) override;
	virtual void OnParticipantLeft(const FString& UPID) override;
	virtual void ApplyInput(const FString& UPID, const FMinigameInput& Input) override;
	virtual void OnAbort() override;
	virtual TSharedRef<class FJsonObject> BuildSnapshot() const override;

	// --- Subclass contract ---
	virtual int32 GetAxisCount() const;
	// Set Axes[i].TargetValue for each axis on start (server-side).
	virtual void InitAxisTargets();

	// --- Designer tuning (editable per BP/instance; §10) ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minigame")
	float AxisToleranceDeg = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minigame")
	float DwellSeconds = 2.5f;

	// Anti-teleport clamp: the largest angle (degrees) a single input message may add.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minigame")
	float MaxStepPerInputDeg = 45.0f;

	UPROPERTY(ReplicatedUsing = OnRep_UpdateAxes, BlueprintReadOnly, Category = "Minigame")
	TArray<FAxisState> Axes;

	UFUNCTION()
	void OnRep_UpdateAxes();

private:
	// Ownership (server).
	void ClaimAxis(const FString& UPID, int32 AxisIndex);
	void ReleaseAxis(const FString& UPID, int32 AxisIndex);
	void ReleaseAxesOf(const FString& UPID);
	void RotateAxis(const FString& UPID, int32 AxisIndex, float DeltaDegrees);

	void EvaluateCompletion(float DeltaTime);

	static float NormalizeDeg(float Angle);
	static float AngularDistanceDeg(float A, float B);
};
