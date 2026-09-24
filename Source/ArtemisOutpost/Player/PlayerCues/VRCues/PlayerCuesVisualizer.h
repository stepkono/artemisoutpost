// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VRCueVisualizer.h"
#include "TrackerPoseTypes.h"
#include "PlayerCuesVisualizer.generated.h"

class AArtemisPlayerState;

/**
 * One remote player's head and controllers, as seen by the local VR player. A vessel: the presenter assigns
 * SourcePlayerState, the data is copied into Pose, and the Blueprint child moves Head / LeftHand / RightHand
 * (anchors frame -> world via UAnchorsManagerSubsystem::GetWorldFromAnchorsFrame, then interpolation).
 */
UCLASS(Blueprintable)
class ARTEMISOUTPOST_API APlayerCuesVisualizer : public AVRCueVisualizer
{
	GENERATED_BODY()

public:
	APlayerCuesVisualizer();

	// Copies SourcePlayerState's replicated tracker pose into Pose. False (Pose untouched) when the source is gone.
	UFUNCTION(BlueprintCallable, Category = "VR Cues")
	bool PullPoseFromSource();

	// True when the source still exists, wears its HMD and Pose is not older than MaxPoseAgeSeconds.
	UFUNCTION(BlueprintPure, Category = "VR Cues")
	bool IsSourceActive(float MaxPoseAgeSeconds = 0.5f) const;

	// Age of Pose in server seconds. Huge when Pose was never written.
	UFUNCTION(BlueprintPure, Category = "VR Cues")
	double GetPoseAgeSeconds() const;

	UPROPERTY(BlueprintReadWrite, Category = "VR Cues")
	TObjectPtr<AArtemisPlayerState> SourcePlayerState;

	// The map key in UVRCuesPresenterComponent. A reconnect gets a new PlayerState and therefore a new id.
	UPROPERTY(BlueprintReadWrite, Category = "VR Cues")
	int32 SourcePlayerId = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, Category = "VR Cues")
	FTrackerPose Pose;

	// Read-only access for the presenter's diagnostics.
	const USceneComponent* GetHeadComponent() const      { return Head; }
	const USceneComponent* GetLeftHandComponent() const  { return LeftHand; }
	const USceneComponent* GetRightHandComponent() const { return RightHand; }

protected:
	// Targets for the Blueprint movement logic. Add the meshes as children of these in the Blueprint.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR Cues")
	TObjectPtr<USceneComponent> Head;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR Cues")
	TObjectPtr<USceneComponent> LeftHand;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR Cues")
	TObjectPtr<USceneComponent> RightHand;

private:
	// Transition-only log guards for PullPoseFromSource.
	bool bLoggedMissingSource = false;
	bool bLoggedWaitingForPose = false;
};
