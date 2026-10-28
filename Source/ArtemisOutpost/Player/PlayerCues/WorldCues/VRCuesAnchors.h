// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VRWorldCue.h"
#include "VRCuesAnchors.generated.h"

/**
 * The physical table as seen by the local player in VR: the shared anchors frame placed in the VR world. A viewer cue
 * (belongs to no player, its Pose stays unused). AnchorsFrame's scale is the table edge length in UE units, so a
 * child mesh of size 1 on X spans one edge.
 */
UCLASS(Blueprintable)
class ARTEMISOUTPOST_API AVRCuesAnchors : public AVRWorldCue
{
	GENERATED_BODY()

public:
	AVRCuesAnchors();
	
	// Reads the live anchors frame from UAnchorsManagerSubsystem into AnchorsFrame. False when not ready.
	UFUNCTION(BlueprintCallable, Category = "World Cues|VR")
	bool RefreshAnchorsFrame();

	UPROPERTY(BlueprintReadWrite, Category = "World Cues|VR")
	FTransform AnchorsFrame = FTransform::Identity;

	UPROPERTY(BlueprintReadOnly, Category = "World Cues|VR")
	bool bHasAnchorsFrame = false;

	// Read-only access for diagnostics. Index 0..3 = A..D, null otherwise.
	const USceneComponent* GetAnchorComponent(int32 Index) const;

protected:
	// Targets for the Blueprint movement logic. Add the meshes as children of these in the Blueprint.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Cues|VR")
	TObjectPtr<USceneComponent> AnchorA;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Cues|VR")
	TObjectPtr<USceneComponent> AnchorB;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Cues|VR")
	TObjectPtr<USceneComponent> AnchorC;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Cues|VR")
	TObjectPtr<USceneComponent> AnchorD;
};
