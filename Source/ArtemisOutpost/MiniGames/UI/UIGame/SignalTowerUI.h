// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArtemisOutpost/MiniGames/UI/UIGame/CoupledAxisMinigameUI.h"
#include "SignalTowerUI.generated.h"

/**
 * C++ base for WBP_UISignalTowerUI. All selection / rotation logic lives in UCoupledAxisMinigameUI,
 * shared with the Habitat View; this class only fixes the tower's content defaults. Axis labels:
 * [0] = Earth, [1] = Habitat (ASignalTower::AxisEarth / AxisHabitat).
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class ARTEMISOUTPOST_API USignalTowerUI : public UCoupledAxisMinigameUI
{
	GENERATED_BODY()

public:
	USignalTowerUI(const FObjectInitializer& ObjectInitializer);
};
