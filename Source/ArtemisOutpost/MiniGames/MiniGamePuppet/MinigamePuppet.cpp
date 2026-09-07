// Fill out your copyright notice in the Description page of Project Settings.

#include "MinigamePuppet.h"

AMinigamePuppet::AMinigamePuppet()
{
	// Pure client-side visual: no replication, no collision needed.
	// Tick IS enabled, because the BP child smooths the pushed axis angle toward its target every
	// frame (ApplyData only arrives at replication cadence, so driving the mesh straight from it
	// makes the ray step). bCanEverTick cannot be turned back on from a Blueprint child, so it has
	// to be true here for the BP's Event Tick to run at all.
	PrimaryActorTick.bCanEverTick = true;
}

void AMinigamePuppet::ApplyData_Implementation(const FInstancedStruct& Data)
{
	PuppetData = Data;
}