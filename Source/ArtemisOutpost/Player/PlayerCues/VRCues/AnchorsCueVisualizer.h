// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VRCueVisualizer.h"
#include "AnchorsCueVisualizer.generated.h"

/**
 * The physical table as seen by the local VR player: the shared anchors frame placed in the VR world.
 * AnchorsFrame's scale is the table edge length in UE units, so a child mesh of size 1 on X spans one edge.
 */
UCLASS(Blueprintable)
class ARTEMISOUTPOST_API AAnchorsCueVisualizer : public AVRCueVisualizer
{
	GENERATED_BODY()

public:
	AAnchorsCueVisualizer();
	
	// Reads the live anchors frame from UAnchorsManagerSubsystem into AnchorsFrame. False when not ready.
	UFUNCTION(BlueprintCallable, Category = "VR Cues")
	bool RefreshAnchorsFrame();

	UPROPERTY(BlueprintReadWrite, Category = "VR Cues")
	FTransform AnchorsFrame = FTransform::Identity;

	UPROPERTY(BlueprintReadOnly, Category = "VR Cues")
	bool bHasAnchorsFrame = false;

	// Read-only access for the presenter's diagnostics. Index 0..3 = A..D, null otherwise.
	const USceneComponent* GetAnchorComponent(int32 Index) const;

protected:
	// Targets for the Blueprint movement logic. Add the meshes as children of these in the Blueprint.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR Cues")
	TObjectPtr<USceneComponent> AnchorA;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR Cues")
	TObjectPtr<USceneComponent> AnchorB;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR Cues")
	TObjectPtr<USceneComponent> AnchorC;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR Cues")
	TObjectPtr<USceneComponent> AnchorD;
};
