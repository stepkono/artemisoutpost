// Fill out your copyright notice in the Description page of Project Settings.

#include "HandToolBase.h"

#include "GameFramework/Pawn.h"
#include "ArtemisOutpost/Player/ACharVR.h"

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

	// Register as the pawn's active tool so BP_VRChar can route the trigger to us (GetActiveTool()).
	if (ACharVR* VR = Cast<ACharVR>(GetOwningPawn()))
	{
		VR->SetActiveHandTool(this);
	}

	OnToolActivated();
}

void AHandToolBase::DeactivateTool()
{
	bToolActive = false;
	SetActorHiddenInGame(true);
	SetActorTickEnabled(false);

	// Clear ourselves as the active tool — but only if we still are (a tool activated afterwards may
	// already have taken over).
	if (ACharVR* VR = Cast<ACharVR>(GetOwningPawn()))
	{
		if (VR->GetActiveTool() == this)
		{
			VR->SetActiveHandTool(nullptr);
		}
	}

	OnToolDeactivated();
}

APawn* AHandToolBase::GetOwningPawn() const
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn)
	{
		Pawn = Cast<APawn>(GetParentActor());
	}
	return Pawn;
}

bool AHandToolBase::IsOwnerLocallyControlled() const
{
	const APawn* Pawn = GetOwningPawn();
	return Pawn && Pawn->IsLocallyControlled();
}

void AHandToolBase::ExecuteAction()
{
	// Base does nothing — override per tool in C++.
}

void AHandToolBase::EndAction()
{
	// Base does nothing — override for hold-style tools (e.g. stop scan on release).
}
