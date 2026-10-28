// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArtemisOutpost/Miscellaneous/DataTypes.h"
#include "TrackerPoseTypes.h"
#include "ArtemisOutpost/Player/PlayerCues/PlayerCueTypes.h"
#include "WorldCue.generated.h"

/**
 * Base for every client-local awareness cue placed in the world (other players, the table frame), in AR and in VR.
 *
 * Pure views: never replicated, no collision, hidden and not ticking until ToggleCueActivation(true). Spawned and
 * switched by the UWorldCuesPresenterComponent of the pawn whose player the cue is about. The presenter decides by
 * the LOCAL viewer's context, the cue's own logic (placement, visuals) lives in the Blueprint children.
 *
 * Vessels: a cue never fetches data. The presenter writes it every tick (WritePose, WritePlayerCueState,
 * WritePlayerIdentity, WriteViewerTrackers), the cue's own tick only turns that data into visuals.
 */
UCLASS(Abstract, Blueprintable)
class ARTEMISOUTPOST_API AWorldCue : public AActor
{
	GENERATED_BODY()

public:
	AWorldCue();

	// Activates (shown + ticking) or deactivates (hidden + not ticking) the cue. Inactive cues get no updates at all,
	// so only the presenter (which keeps ticking) may switch them back on.
	UFUNCTION(BlueprintCallable, Category = "World Cues")
	void ToggleCueActivation(bool bActivate);

	UFUNCTION(BlueprintPure, Category = "World Cues")
	bool IsCueActive() const { return bCueActive; }

	// The viewer context this cue is meant for (AR cues: AR, VR cues: VR). Set by the AR / VR base classes.
	UFUNCTION(BlueprintPure, Category = "World Cues")
	ERVMode GetLocalViewerCueContext() const { return CueContext; }

	// ---- Data, written by the presenter ----

	// The tracker pose of the player this cue is about (anchors-frame fractions, from AArtemisPlayerState).
	UFUNCTION(BlueprintCallable, Category = "World Cues")
	void WritePose(const FTrackerPose& InPose) { Pose = InPose; }

	// The cue state of the player this cue is about (context, HMD, activity, talking, pointing, ...).
	// Not to be confused with GetLocalViewerCueContext, the viewer context this cue is shown in.
	UFUNCTION(BlueprintCallable, Category = "World Cues")
	void WritePlayerCueState(const FPlayerCueState& InCueState) { PlayerCueState = InCueState; }

	// UPID, number and colour of the player this cue is about.
	UFUNCTION(BlueprintCallable, Category = "World Cues")
	void WritePlayerIdentity(const FCuePlayerIdentity& InIdentity) { PlayerIdentity = InIdentity; }

	// The local viewer's own trackers, world space.
	UFUNCTION(BlueprintCallable, Category = "World Cues")
	void WriteViewerTrackers(const FCueLocalViewerTrackers& InTrackers) { ViewerTrackers = InTrackers; }

	// Age of Pose in server seconds. Huge when Pose was never written.
	UFUNCTION(BlueprintPure, Category = "World Cues")
	double GetPoseAgeSeconds() const;

	UPROPERTY(BlueprintReadOnly, Category = "World Cues")
	FTrackerPose Pose;

	UPROPERTY(BlueprintReadOnly, Category = "World Cues")
	FPlayerCueState PlayerCueState;

	UPROPERTY(BlueprintReadOnly, Category = "World Cues")
	FCuePlayerIdentity PlayerIdentity;

	UPROPERTY(BlueprintReadOnly, Category = "World Cues")
	FCueLocalViewerTrackers ViewerTrackers;

	// ---- Placement ----

	// Moves TrackerCue towards NewTransform (anchors-frame fractions, e.g. Pose.Head) converted to world, location and
	// rotation interpolated with TrackerInterpSpeed. False (TrackerCue untouched) while TrackerCue is null or the
	// anchors frame is not ready.
	UFUNCTION(BlueprintCallable, Category = "World Cues")
	bool UpdateTrackerTransform(USceneComponent* TrackerCue, const FTransform& NewTransform, float DeltaTime);

	// VInterpTo / RInterpTo speed of UpdateTrackerTransform. 0 = snap.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Cues", meta = (ClampMin = "0.0"))
	float TrackerInterpSpeed = 15.0f;

	// ---- Change detection (e.g. widgets that only redraw when their data changed) ----

	// True when every field of A and B is equal (PointerTarget field by field). GazeTarget is NOT compared yet.
	UFUNCTION(BlueprintPure, Category = "World Cues")
	static bool CheckEqualityForPlayerCueState(const FPlayerCueState& A, const FPlayerCueState& B);

	// True when UPID, PlayerNumber and Color of A and B are equal.
	UFUNCTION(BlueprintPure, Category = "World Cues")
	static bool CheckEqualityForPlayerIdentity(const FCuePlayerIdentity& A, const FCuePlayerIdentity& B);

protected:
	// Blueprint hook, e.g. to snap instead of interpolating when the cue gets activated again.
	UFUNCTION(BlueprintImplementableEvent, Category = "World Cues")
	void OnCueActivationChanged(bool bActivate);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Cues")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "World Cues")
	ERVMode CueContext = ERVMode::AR;

private:
	bool bCueActive = false;
};
