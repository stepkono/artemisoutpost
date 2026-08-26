// Fill out your copyright notice in the Description page of Project Settings.

#include "MinigamePuppet.h"

AMinigamePuppet::AMinigamePuppet()
{
	// Pure client-side visual: no tick, no replication, no collision needed.
	PrimaryActorTick.bCanEverTick = false;
}

void AMinigamePuppet::SetData_Implementation(const FPuppetData ParentData)
{
	PuppetData = ParentData;
}
