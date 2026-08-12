// Fill out your copyright notice in the Description page of Project Settings.

#include "SignalTower.h"
#include "SignalTowerLogicComponent.h"

ASignalTower::ASignalTower()
{
	// The base already created the Connection component. Add the concrete task logic; the base
	// resolves it into AMinigameActor::Logic in BeginPlay.
	GameLogic = CreateDefaultSubobject<USignalTowerLogicComponent>(TEXT("Alignment"));
}