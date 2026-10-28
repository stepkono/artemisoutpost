// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ARWorldCue.h"
#include "ARCuesPlayer.generated.h"

/**
 * One remote player's head and controllers, as seen by the local player in AR (e.g. a widget above the head).
 * A vessel like AVRCuesPlayer: the presenter writes Pose and ViewerTrackers every tick, the Blueprint child places
 * Head / LeftHand / RightHand and does the visuals.
 */
UCLASS(Blueprintable)
class ARTEMISOUTPOST_API AARCuesPlayer : public AARWorldCue
{
	GENERATED_BODY()

public:
	AARCuesPlayer();

	// Read-only access for diagnostics.
	const USceneComponent* GetHeadComponent() const      { return Head; }
	const USceneComponent* GetLeftHandComponent() const  { return LeftHand; }
	const USceneComponent* GetRightHandComponent() const { return RightHand; }

protected:
	// Targets for the Blueprint placement logic. Add the meshes / widgets as children of these in the Blueprint.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Cues|AR")
	TObjectPtr<USceneComponent> Head;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Cues|AR")
	TObjectPtr<USceneComponent> LeftHand;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Cues|AR")
	TObjectPtr<USceneComponent> RightHand;
};
