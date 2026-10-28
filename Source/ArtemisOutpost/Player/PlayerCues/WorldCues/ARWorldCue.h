// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WorldCue.h"
#include "ARWorldCue.generated.h"

/**
 * Base for the world cues the local player sees while in AR. Shown by the presenter only while the local viewer's
 * context is AR (CueContext = AR).
 */
UCLASS(Abstract, Blueprintable)
class ARTEMISOUTPOST_API AARWorldCue : public AWorldCue
{
	GENERATED_BODY()

public:
	AARWorldCue();
};
