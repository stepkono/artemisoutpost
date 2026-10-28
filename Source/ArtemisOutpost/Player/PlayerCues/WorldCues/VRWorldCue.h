// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WorldCue.h"
#include "VRWorldCue.generated.h"

/**
 * Base for the world cues the local player sees while in VR. Shown by the presenter only while the local viewer's
 * context is VR (CueContext = VR).
 */
UCLASS(Abstract, Blueprintable)
class ARTEMISOUTPOST_API AVRWorldCue : public AWorldCue
{
	GENERATED_BODY()

public:
	AVRWorldCue();

protected:
	// ONLY for poses read directly from the raw anchor ACTORS: brings them into the camera's space via
	// UAnchorsManagerSubsystem::RawAnchorToTrackedSpace (VR: removes the extra tilt MetaXR adds, identity in AR). Do NOT
	// use it on anything from the anchors FRAME (GetWorldFromAnchorsFrame, AnchorsFrame): TryGetAnchorsFrame already
	// puts the frame in the camera's space. The scale is reset to 1 on purpose (sent transforms carry the sender's scale).
	UFUNCTION(BlueprintPure, Category = "World Cues|VR")
	FTransform ApplyXRBaseTilt(const FTransform& RawTransform) const;
};
