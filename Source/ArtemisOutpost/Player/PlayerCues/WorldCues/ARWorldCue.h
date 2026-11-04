// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WorldCue.h"
#include "ARWorldCue.generated.h"

class UVisualScaleSyncComponent;

/**
 * Base for the world cues the local player sees while in AR. Shown by the presenter only while the local viewer's
 * context is AR (CueContext = AR).
 *
 * AR zoom: VisualScaleSync fires OnVisualScaleChanged on every zoom step and once right after spawn. Rescale /
 * reposition the cue there (Details panel of the component, "+" next to the event) and do NOT bind
 * UTransformationsManager::OnWTMShouldResize in the Blueprint as well.
 */
UCLASS(Abstract, Blueprintable)
class ARTEMISOUTPOST_API AARWorldCue : public AWorldCue
{
	GENERATED_BODY()

public:
	AARWorldCue();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Cues|AR")
	TObjectPtr<UVisualScaleSyncComponent> VisualScaleSync;
};
