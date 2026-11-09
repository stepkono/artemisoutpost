// Fill out your copyright notice in the Description page of Project Settings.

#include "HandToolBase.h"

#include "GameFramework/Pawn.h"
#include "ArtemisOutpost/Player/PawnVR/ACharVR.h"
#include "ArtemisOutpost/Player/PlayerCues/PlayerCuesManager.h"
#include "ArtemisOutpost/Player/PlayerController/PlayerControllerComponents/InputMode/PlayerInputModeComponent.h"

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

	// Enter the HandTool activity BEFORE registering: the owner holsters a different drawn tool, which it
	// finds as the pawn's current active tool. Only the holder reports.
	if (IsOwnerLocallyControlled())
	{
		if (UPlayerInputModeComponent* InputMode = UPlayerInputModeComponent::FindFor(GetOwningPawn()))
		{
			InputMode->EnterHandTool(this);
		}
	}

	// Register as the pawn's active tool so the trigger reaches us (UPlayerInputModeComponent::HandleTrigger).
	if (ACharVR* VR = Cast<ACharVR>(GetOwningPawn()))
	{
		VR->SetActiveHandTool(this);
	}

	ReportToolActivity(true);

	OnToolActivated();
}

void AHandToolBase::ReportToolActivity(bool bActive) const
{
	// Only the player holding the tool reports; every other instance just shows the mesh.
	if (!IsOwnerLocallyControlled())
	{
		return;
	}
	const APawn* Pawn = GetOwningPawn();
	UPlayerCuesManager* Cues = Pawn ? Pawn->FindComponentByClass<UPlayerCuesManager>() : nullptr;
	if (Cues)
	{
		Cues->ReportToolActivity(bActive ? GetToolActivity() : EToolActivity::None);
	}
}

void AHandToolBase::DeactivateTool()
{
	bToolActive = false;
	SetActorHiddenInGame(true);
	SetActorTickEnabled(false);

	// Clear ourselves as the active tool — but only if we still are (a tool activated afterwards may
	// already have taken over).
	bool bWasActiveTool = false;
	if (ACharVR* VR = Cast<ACharVR>(GetOwningPawn()))
	{
		if (VR->GetActiveTool() == this)
		{
			VR->SetActiveHandTool(nullptr);
			bWasActiveTool = true;
		}
	}

	ReportToolActivity(false);

	OnToolDeactivated();

	// Holstered by the player (HUD tile, BP HolsterTool): back to Free. When the owner itself holsters us
	// in a teardown, its re-entrancy guard ignores this. A tool that was not the active one changes nothing.
	if (bWasActiveTool && IsOwnerLocallyControlled())
	{
		if (UPlayerInputModeComponent* InputMode = UPlayerInputModeComponent::FindFor(GetOwningPawn()))
		{
			InputMode->LeaveActivity(EInputActivity::HandTool, FString::Printf(TEXT("%s holstered"), *GetName()));
		}
	}
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
