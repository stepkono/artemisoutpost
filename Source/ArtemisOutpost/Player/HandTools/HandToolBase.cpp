// Fill out your copyright notice in the Description page of Project Settings.

#include "HandToolBase.h"

#include "GameFramework/Pawn.h"

AHandToolBase::AHandToolBase()
{
	// The BP child's aiming/trace logic runs on Tick, but only while the tool is active.
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(SceneRoot);
}

void AHandToolBase::BeginPlay()
{
	Super::BeginPlay();

	// Tools start holstered (hidden + not ticking); a mode selection activates the right one.
	DeactivateTool();
}

void AHandToolBase::ActivateTool()
{
	bToolActive = true;

	// Show the mesh on every client (so remote players see the held tool). But the per-frame aiming /
	// trace logic is meaningful only for the player actually holding it, so tick only there.
	SetActorHiddenInGame(false);
	SetActorTickEnabled(IsOwnerLocallyControlled());

	OnToolActivated();
}

bool AHandToolBase::IsOwnerLocallyControlled() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn)
	{
		Pawn = Cast<APawn>(GetParentActor());
	}
	return Pawn && Pawn->IsLocallyControlled();
}

void AHandToolBase::DeactivateTool()
{
	bToolActive = false;
	SetActorHiddenInGame(true);
	SetActorTickEnabled(false);
	OnToolDeactivated();
}
