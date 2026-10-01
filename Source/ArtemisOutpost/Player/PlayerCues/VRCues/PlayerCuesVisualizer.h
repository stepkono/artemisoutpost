// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VRCueVisualizer.h"
#include "TrackerPoseTypes.h"
#include "PlayerCuesVisualizer.generated.h"

class AArtemisPlayerState;
class UVRCuesPresenterComponent;

// The LOCAL player's trackers (the viewer), world space. Weights: 1 = counts, 0 = untracked (ignored by the net).
USTRUCT(BlueprintType)
struct FCueLocalTrackers
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR Cues")
	FVector Head = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR Cues")
	FVector Left = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR Cues")
	FVector Right = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR Cues")
	float HeadWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR Cues")
	float LeftWeight = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR Cues")
	float RightWeight = 0.0f;
};

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

	// Called by the presenter every tick while active. The Blueprint does the net logic there (cylinder fit, radius
	// smoothing, transform, material offsets, render gate) with its own components, variables and the tunables below,
	// so the presenter never has to cast.
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "VR Cues|Net")
	void OnNetCylinderUpdate(const FCueLocalTrackers& LocalTrackers);

	// Read-only access for the presenter's diagnostics.
	const USceneComponent* GetHeadComponent() const      { return Head; }
	const USceneComponent* GetLeftHandComponent() const  { return LeftHand; }
	const USceneComponent* GetRightHandComponent() const { return RightHand; }
	
	UPROPERTY(BlueprintReadWrite, Category = "VR Cues")
	bool bHasNetRadius = false;

	// ---- Net cylinder tunables, edited in the BP_PlayerCuesVisualizer defaults. All lengths in UE units (cm). ----
	// Axis = moon center -> head, top edge at head + HeadClearance, radius = the larger horizontal head->controller
	// distance (untracked controllers ignored).

	// Length along the axis, extending down from the top edge.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR Cues|Net Cylinder", meta = (ClampMin = "1.0"))
	float CylinderLength = 150.0f;

	// How far the top edge sits above the head.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR Cues|Net Cylinder")
	float HeadClearance = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR Cues|Net Cylinder", meta = (ClampMin = "1.0"))
	float MinRadius = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR Cues|Net Cylinder", meta = (ClampMin = "1.0"))
	float MaxRadius = 90.0f;

	// Used while neither controller is tracked and no radius was measured yet.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR Cues|Net Cylinder", meta = (ClampMin = "1.0"))
	float DefaultRadius = 35.0f;

	// FInterpTo speed of the radius. 0 = no smoothing.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR Cues|Net Cylinder", meta = (ClampMin = "0.0"))
	float RadiusInterpSpeed = 6.0f;

	// The lines start to glow when one of the local trackers is closer than this.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR Cues|Proximity", meta = (ClampMin = "0.0"))
	float ProximityThreshold = 40.0f;

	// Full glow at this distance or closer. Must stay below ProximityThreshold.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR Cues|Proximity", meta = (ClampMin = "0.0"))
	float ProximityFullGlow = 10.0f;

	// The cylinder mesh is only rendered while a local tracker is within ProximityThreshold + GateMargin of its wall.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR Cues|Proximity", meta = (ClampMin = "0.0"))
	float GateMargin = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR Cues|Net Look")
	FLinearColor LineColor = FLinearColor(0.0f, 0.8f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR Cues|Net Look", meta = (ClampMin = "0.0"))
	float GlowIntensity = 1.5f;

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
