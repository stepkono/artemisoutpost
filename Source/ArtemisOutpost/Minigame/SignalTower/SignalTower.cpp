// Fill out your copyright notice in the Description page of Project Settings.

#include "ArtemisOutpost/Minigame/SignalTower/SignalTower.h"
#include "ArtemisOutpost/Minigame/SignalTower/SignalTowerLogicComponent.h"

ASignalTower::ASignalTower()
{
	// The base already created the Connection component. Add the concrete task logic; the base
	// resolves it into AMinigameActor::Logic in BeginPlay.
	Alignment = CreateDefaultSubobject<USignalTowerLogicComponent>(TEXT("Alignment"));
}

USignalTowerLogicComponent* ASignalTower::GetAlignment() const
{
	return Alignment;
}
