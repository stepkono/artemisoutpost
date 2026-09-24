// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VRCueVisualizer.generated.h"

/**
 * Base for the client-local awareness visualizers the VR player sees (other players' trackers, the table frame).
 *
 * Pure views: never replicated, no collision, hidden and not ticking until SetCueVisible(true). The data is
 * filled in by UVRCuesPresenterComponent, the movement logic lives in the Blueprint children.
 */
UCLASS(Abstract, Blueprintable)
class ARTEMISOUTPOST_API AVRCueVisualizer : public AActor
{
	GENERATED_BODY()

public:
	AVRCueVisualizer();

	// Shows or hides the visualizer and enables or disables its tick together with it.
	UFUNCTION(BlueprintCallable, Category = "VR Cues")
	void SetCueVisible(bool bVisible);

	UFUNCTION(BlueprintPure, Category = "VR Cues")
	bool IsCueVisible() const { return bCueVisible; }
	
	// Legacy: ApplyXRBaseTilt reads the tilt from the XR system itself now. Kept so existing Blueprint calls compile.
	UFUNCTION(BlueprintCallable, Category = "VR Cues", meta = (DeprecatedFunction, DeprecationMessage = "No longer needed: ApplyXRBaseTilt reads the tilt from the XR system. Remove this call."))
	void SetXRTrackingRotation(FVector Pivot, FRotator Tilt);

protected:
	// Blueprint hook, e.g. to snap instead of interpolating when the visualizer reappears.
	UFUNCTION(BlueprintImplementableEvent, Category = "VR Cues")
	void OnCueVisibilityChanged(bool bVisible);
	
	// ONLY for poses read directly from the raw anchor ACTORS: brings them into the camera's space via
	// UAnchorsManagerSubsystem::RawAnchorToTrackedSpace (VR: removes the extra tilt MetaXR adds, identity in AR). Do NOT
	// use it on anything from the anchors FRAME (GetWorldFromAnchorsFrame, AnchorsFrame): TryGetAnchorsFrame already
	// puts the frame in the camera's space. The scale is reset to 1 on purpose (sent transforms carry the sender's scale).
	UFUNCTION(BlueprintPure, Category = "VR Cues")
	FTransform ApplyXRBaseTilt(const FTransform& RawTransform) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR Cues")
	TObjectPtr<USceneComponent> Root;

	// Legacy, filled by SetXRTrackingRotation only and no longer used by ApplyXRBaseTilt. Kept so Blueprint reads compile.
	UPROPERTY(BlueprintReadOnly, Category = "VR Cues")
	FVector XRTrackingPivot = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "VR Cues")
	FRotator XRTrackingTilt = FRotator::ZeroRotator;

private:
	bool bCueVisible = false;
};
