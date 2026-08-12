// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArtemisOutpost/Minigame/General/GameInstance/MinigameActor.h"
#include "SignalTower.generated.h"

class USignalTowerLogicComponent;

// The radio mast (Funkmast). Pure composition: wires the reusable connection + alignment logic
// onto the base minigame actor. Meshes, the world-space alignment widget, and the AR/VR beams
// live on BP_SignalTower, which derives from this and reads the replicated axes (via the
// alignment component's OnAxesUpdated event).
UCLASS()
class ARTEMISOUTPOST_API ASignalTower : public AMinigameActor
{
	GENERATED_BODY()

public:
	ASignalTower();
	
};
