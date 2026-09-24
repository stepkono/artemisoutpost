// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VRCuesPresenterComponent.generated.h"

class AArtemisPlayerState;
class APlayerCuesVisualizer;
class AAnchorsCueVisualizer;

/**
 * Central manager of the VR awareness visualizers on the local ACharVR.
 *
 * ACharVR switches it on and off (SetPresenting) whenever it starts or stops driving the local VR view, so it
 * never runs on other players' VR proxies and never in AR. Switching off destroys every visualizer and clears
 * the map, so the map is rebuilt on every switch to VR. While presenting, the Blueprint child lazily spawns one
 * APlayerCuesVisualizer per remote PlayerState into PlayerCuesVisualizers (keyed by PlayerId) plus the single
 * AnchorsVisualizer, fills them each tick and calls PruneInvalidVisualizers for players that left.
 */
UCLASS(Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ARTEMISOUTPOST_API UVRCuesPresenterComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVRCuesPresenterComponent();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Runs the Blueprint child's Event Tick first, then the throttled diagnostics summary.
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// While presenting, logs one summary line for the tilt/anchors and one per remote player (source state, pose age,
	// where the visualizer components actually ended up). Every SummaryLogInterval seconds.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR Cues|Debug")
	bool bLogSummary = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR Cues|Debug", meta = (ClampMin = "0.1"))
	float SummaryLogInterval = 1.0f;

	// On: enables tick. Off: disables tick, destroys all visualizers and clears the map.
	UFUNCTION(BlueprintCallable, Category = "VR Cues")
	void SetPresenting(bool bInPresenting);

	UFUNCTION(BlueprintPure, Category = "VR Cues")
	bool IsPresenting() const { return bPresenting; }

	// Destroys and removes visualizers whose actor or source PlayerState is gone. Returns how many were removed.
	UFUNCTION(BlueprintCallable, Category = "VR Cues")
	int32 PruneInvalidVisualizers();

	UFUNCTION(BlueprintCallable, Category = "VR Cues")
	void DestroyAllVisualizers();

	// Every AArtemisPlayerState in GameState->PlayerArray except the local player's own.
	UFUNCTION(BlueprintCallable, Category = "VR Cues")
	TArray<AArtemisPlayerState*> GetRemotePlayerStates() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VR Cues")
	TSubclassOf<APlayerCuesVisualizer> PlayerCuesClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VR Cues")
	TSubclassOf<AAnchorsCueVisualizer> AnchorsCuesClass;

	// Keyed by APlayerState::GetPlayerId. Not persistent: cleared whenever presenting stops.
	UPROPERTY(Transient, BlueprintReadWrite, Category = "VR Cues")
	TMap<int32, TObjectPtr<APlayerCuesVisualizer>> PlayerCuesVisualizers;

	UPROPERTY(Transient, BlueprintReadWrite, Category = "VR Cues")
	TObjectPtr<AAnchorsCueVisualizer> AnchorsVisualizer;

protected:
	// Blueprint hook after the presenting state changed (tick and cleanup are already done).
	UFUNCTION(BlueprintImplementableEvent, Category = "VR Cues")
	void OnPresentingChanged(bool bIsPresenting);

private:
	void LogSummary() const;

	bool bPresenting = false;
	float SummaryLogTimer = 0.0f;

	// Transition-only warnings from GetRemotePlayerStates (const, hence mutable).
	mutable bool bWarnedNoGameState = false;
	mutable bool bWarnedNoOwnPlayerState = false;
	mutable TSet<int32> LoggedSkippedPlayerIds;
};
