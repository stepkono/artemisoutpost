// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArtemisOutpost/Minigame/MinigameLogicComponent.h"
#include "CoupledAxisLogicComponent.generated.h"

// Reusable "two people, two locked degrees of freedom" rules. Signal Tower (Earth + Habitat)
// and Habitat (Pitch + Roll) both derive from this. With one participant all axes are freely
// controllable; with two the axes are partitioned one-per-person and neither can move the
// other's. Completes when ALL axes hold within tolerance for DwellSeconds at once.
UCLASS(Abstract, ClassGroup = (Minigame))
class ARTEMISOUTPOST_API UCoupledAxisLogicComponent : public UMinigameLogicComponent
{
	GENERATED_BODY()

public:
	UCoupledAxisLogicComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintPure, Category = "Minigame")
	const TArray<FAxisState>& GetAxes() const;

	// 0..1 dwell fraction of an axis, for UI / awareness cues.
	UFUNCTION(BlueprintPure, Category = "Minigame")
	float GetAxisProgress(int32 AxisIndex) const;

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

	// The single view hook. Fires on every peer with the current axes (beams / mesh / UI read
	// straight from this): on the server directly from OnStart/ApplyInput/OnAbort, on remote
	// clients from OnRep_Axes.
	UFUNCTION(BlueprintImplementableEvent, Category = "Minigame")
	void OnAlignmentUpdated(const TArray<FAxisState>& CurrentAxes);

	// --- Designer tuning (editable per BP/instance; §10) ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minigame")
	float AxisToleranceDeg = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minigame")
	float DwellSeconds = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minigame")
	float InputSpeedDegPerSec = 45.0f;

	UPROPERTY(ReplicatedUsing = OnRep_UpdateAxes, BlueprintReadOnly, Category = "Minigame")
	TArray<FAxisState> Axes;

	UFUNCTION()
	void OnRep_UpdateAxes();

private:
	bool CanControlAxis(const FString& UPID, int32 AxisIndex) const;
	void ReassignAxisOwnership();
	void EvaluateCompletion(float DeltaTime);

	static float NormalizeDeg(float Angle);
	static float AngularDistanceDeg(float A, float B);
};
