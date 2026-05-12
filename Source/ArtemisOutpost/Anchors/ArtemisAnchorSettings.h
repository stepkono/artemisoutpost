// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ArtemisAnchorSettings.generated.h"

/**
 * Project-wide anchor configuration. Editable in Project Settings -> Game -> Artemis Anchor Settings.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Artemis Anchor Settings"))
class ARTEMISOUTPOST_API UArtemisAnchorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Blueprint class used to spawn spatial anchor actors. Assign BP_SpatialAnchorModel here. */
	UPROPERTY(Config, EditAnywhere, Category="Anchors")
	TSoftClassPtr<AActor> SpatialAnchorModelClass;
};
