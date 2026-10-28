// Fill out your copyright notice in the Description page of Project Settings.

#include "WorldCue.h"

#include "Components/SceneComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "ArtemisOutpost/Anchors/AnchorsManagerSubsystem.h"

AWorldCue::AWorldCue()
{
	// Client-local view of replicated data, nothing about it goes over the network.
	bReplicates = false;
	SetActorEnableCollision(false);
	SetActorHiddenInGame(true);

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void AWorldCue::ToggleCueActivation(bool bActivate)
{
	if (bCueActive == bActivate)
	{
		return;
	}
	bCueActive = bActivate;

	SetActorHiddenInGame(!bActivate);
	SetActorTickEnabled(bActivate);

	UE_LOG(LogTemp, Log, TEXT("[WorldCues] %s active=%d"), *GetName(), bActivate ? 1 : 0);
	OnCueActivationChanged(bActivate);
}

double AWorldCue::GetPoseAgeSeconds() const
{
	return Pose.GetAgeSeconds(GetWorld());
}

bool AWorldCue::UpdateTrackerTransform(USceneComponent* TrackerCue, const FTransform& NewTransform, float DeltaTime)
{
	if (!TrackerCue)
	{
		return false;
	}

	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const UAnchorsManagerSubsystem* AnchorsManager = GameInstance ? GameInstance->GetSubsystem<UAnchorsManagerSubsystem>() : nullptr;

	// Without a frame GetWorldFromAnchorsFrame returns the fractions unchanged, which would pull the cue to the world origin.
	if (!AnchorsManager || !AnchorsManager->HasValidAnchorsFrame())
	{
		return false;
	}

	const FTransform Target = AnchorsManager->GetWorldFromAnchorsFrame(NewTransform);

	const FVector  NewLocation = FMath::VInterpTo(TrackerCue->GetComponentLocation(), Target.GetLocation(), DeltaTime, TrackerInterpSpeed);
	const FRotator NewRotation = FMath::RInterpTo(TrackerCue->GetComponentRotation(), Target.Rotator(), DeltaTime, TrackerInterpSpeed);
	TrackerCue->SetWorldLocationAndRotation(NewLocation, NewRotation);
	return true;
}

bool AWorldCue::CheckEqualityForPlayerCueState(const FPlayerCueState& A, const FPlayerCueState& B)
{
	if (A.Context      != B.Context)      return false;
	if (A.bHmdWorn     != B.bHmdWorn)     return false;
	if (A.bInMinigame  != B.bInMinigame)  return false;
	if (A.MinigameType != B.MinigameType) return false;
	if (A.ToolActivity != B.ToolActivity) return false;
	if (A.bWalking     != B.bWalking)     return false;
	if (A.bTalking     != B.bTalking)     return false;
	if (A.bPointing    != B.bPointing)    return false;
	if (A.PointingHand != B.PointingHand) return false;

	const FPointingTarget& PointerA = A.PointerTarget;
	const FPointingTarget& PointerB = B.PointerTarget;
	if (PointerA.Kind               != PointerB.Kind)               return false;
	if (PointerA.GeoHit             != PointerB.GeoHit)             return false;
	if (PointerA.MinigameMGID       != PointerB.MinigameMGID)       return false;
	if (PointerA.MinigameType       != PointerB.MinigameType)       return false;
	if (PointerA.TargetUPID         != PointerB.TargetUPID)         return false;
	if (PointerA.TargetPlayerNumber != PointerB.TargetPlayerNumber) return false;

	// TODO: GazeTarget is left out for now. Review the gaze update rate (GazeTraceInterval / GazeDwellSeconds in
	// UPlayerCuesManager) before comparing it here, so widgets do not redraw at gaze rate.
	return true;
}

bool AWorldCue::CheckEqualityForPlayerIdentity(const FCuePlayerIdentity& A, const FCuePlayerIdentity& B)
{
	if (A.UPID         != B.UPID)         return false;
	if (A.PlayerNumber != B.PlayerNumber) return false;
	if (A.Color        != B.Color)        return false;
	return true;
}
