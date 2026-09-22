// Fill out your copyright notice in the Description page of Project Settings.

#include "ArtemisPlayerState.h"

#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "ArtemisOutpost/Player/PawnVR/ACharVR.h"
#include "ArtemisOutpost/Player/PawnAR/PawnAR.h"
#include "ArtemisOutpost/Player/Rover/AMasterRover.h"
#include "ArtemisOutpost/StudyData/Providers/PlayerActionProvider/PlayerActionProvider.h"

AArtemisPlayerState::AArtemisPlayerState()
{
	// APlayerState defaults to 1 Hz. Pointer beacons on other clients would crawl at one update per
	// second, so raise it; four players with one small struct each is negligible bandwidth.
	SetNetUpdateFrequency(20.0f);
	SetMinNetUpdateFrequency(2.0f);
}

void AArtemisPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AArtemisPlayerState, UPID);
	DOREPLIFETIME(AArtemisPlayerState, PlayerNumber);
	DOREPLIFETIME(AArtemisPlayerState, VRChar);
	DOREPLIFETIME(AArtemisPlayerState, ARPawn);
	DOREPLIFETIME(AArtemisPlayerState, MasterRover);
	DOREPLIFETIME(AArtemisPlayerState, CueState);
}

// ---- Identity ----

void AArtemisPlayerState::ServerSetUPID(const FString& InUPID)
{
	if (!HasAuthority())
	{
		return;
	}
	UPID = InUPID;
	UE_LOG(LogTemp, Log, TEXT("[Cues] PlayerState %s now carries UPID '%s'."), *GetName(), *UPID);
}

void AArtemisPlayerState::ServerSetPlayerNumber(int32 InNumber)
{
	if (!HasAuthority())
	{
		return;
	}
	PlayerNumber = InNumber;
}

void AArtemisPlayerState::ServerSetPawns(ACharVR* InVRChar, APawnAR* InARPawn, AMasterRover* InRover)
{
	if (!HasAuthority())
	{
		return;
	}
	// Only overwrite what the caller actually knows; the AR pawn and the VR pawns arrive on different paths.
	if (InVRChar) { VRChar = InVRChar; }
	if (InARPawn) { ARPawn = InARPawn; }
	if (InRover)  { MasterRover = InRover; }
}

// ---- Server mutators ----

void AArtemisPlayerState::ServerSetXRMode(EXRMode Mode)
{
	if (!HasAuthority())
	{
		return;
	}
	LastXRMode = Mode;
	RecomputeContext();
}

void AArtemisPlayerState::ServerSetHmdWorn(bool bWorn)
{
	if (!HasAuthority() || CueState.bHmdWorn == bWorn)
	{
		return;
	}
	CueState.bHmdWorn = bWorn;
	RecomputeContext();
}

void AArtemisPlayerState::RecomputeContext()
{
	const EPlayerContext NewContext = !CueState.bHmdWorn
		? EPlayerContext::R
		: (LastXRMode == EXRMode::VR ? EPlayerContext::VR : EPlayerContext::AR);

	const bool bChanged = (NewContext != CueState.Context);
	CueState.Context = NewContext;

	if (bChanged)
	{
		UE_LOG(LogTemp, Log, TEXT("[Cues] '%s' context -> %s"), *UPID, *UEnum::GetValueAsString(NewContext));
		EmitEvent(EGameEventType::PlayerContextChanged);
	}
	CommitChange();
}

void AArtemisPlayerState::ServerSetTalking(bool bTalking)
{
	if (!HasAuthority() || CueState.bTalking == bTalking)
	{
		return;
	}
	CueState.bTalking = bTalking;
	EmitEvent(bTalking ? EGameEventType::PlayerTalkStart : EGameEventType::PlayerTalkFinish);
	CommitChange();
}

void AArtemisPlayerState::ServerSetPointing(EPointingHand Hand, bool bPointing)
{
	if (!HasAuthority())
	{
		return;
	}
	if (CueState.bPointing == bPointing && (!bPointing || CueState.PointingHand == Hand))
	{
		return;
	}

	CueState.bPointing = bPointing;
	CueState.PointingHand = Hand;
	if (!bPointing)
	{
		EmitEvent(EGameEventType::PlayerPointingFinish);
		CueState.PointerTarget = FPointingTarget();
	}
	else
	{
		// Start is emitted once the first target arrives (ServerSetPointerTarget), so the start event
		// already carries a position. Clear any stale target from the previous press.
		CueState.PointerTarget = FPointingTarget();
		LastPointingUpdateTime = -1000.0;
	}
	CommitChange();
}

void AArtemisPlayerState::ServerSetPointerTarget(const FPointingTarget& Target)
{
	if (!HasAuthority() || !CueState.bPointing)
	{
		return;
	}

	const bool bHadTarget   = CueState.PointerTarget.IsValid();
	const bool bSameThing   = CueState.PointerTarget.IsSameTarget(Target);
	const bool bMoved       = !CueState.PointerTarget.GeoHit.Equals(Target.GeoHit, 1e-7);

	CueState.PointerTarget = Target;

	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (!bHadTarget && Target.IsValid())
	{
		EmitEvent(EGameEventType::PlayerPointingStart);
		LastPointingUpdateTime = Now;
	}
	else if (!bSameThing)
	{
		EmitEvent(EGameEventType::PlayerPointingUpdate);
		LastPointingUpdateTime = Now;
	}
	else if (bMoved && (Now - LastPointingUpdateTime) >= PointingUpdateInterval)
	{
		EmitEvent(EGameEventType::PlayerPointingUpdate);
		LastPointingUpdateTime = Now;
	}
	CommitChange();
}

void AArtemisPlayerState::ServerSetGazeTarget(const FPointingTarget& Target)
{
	if (!HasAuthority())
	{
		return;
	}
	const bool bSameThing = CueState.GazeTarget.IsSameTarget(Target);
	const bool bWasValid  = CueState.GazeTarget.IsValid();

	// The client only reports on a target change (after its dwell), so every call here that names a
	// different thing is a Finish for the old one (emitted while the struct still holds it) and a
	// Start for the new one.
	if (!bSameThing && bWasValid)
	{
		EmitEvent(EGameEventType::PlayerLookAtFinish);
	}

	CueState.GazeTarget = Target;

	if (!bSameThing && Target.IsValid())
	{
		EmitEvent(EGameEventType::PlayerLookAtStart);
	}
	CommitChange();
}

void AArtemisPlayerState::ServerSetToolActivity(EToolActivity Tool)
{
	if (!HasAuthority() || CueState.ToolActivity == Tool)
	{
		return;
	}
	CueState.ToolActivity = Tool;
	CommitChange();
}

void AArtemisPlayerState::ServerSetMinigame(bool bInMinigame, EMiniGameType Type)
{
	if (!HasAuthority())
	{
		return;
	}
	if (CueState.bInMinigame == bInMinigame && (!bInMinigame || CueState.MinigameType == Type))
	{
		return;
	}
	CueState.bInMinigame = bInMinigame;
	CueState.MinigameType = Type;
	CommitChange();
}

void AArtemisPlayerState::ServerSetWalking(bool bWalking)
{
	if (!HasAuthority() || CueState.bWalking == bWalking)
	{
		return;
	}
	CueState.bWalking = bWalking;
	CommitChange();
}

// ---- Change plumbing ----

void AArtemisPlayerState::CommitChange()
{
	const EPlayerActivity Activity = CueState.GetActivity();
	if (Activity != LastEmittedActivity)
	{
		LastEmittedActivity = Activity;
		EmitEvent(EGameEventType::PlayerActivityChanged);
	}

	// The server has no OnRep, so broadcast here for the host-side listeners (server cue managers).
	OnCueStateChanged.Broadcast(CueState);

	// Struct replication is change-detected per property; forcing an update keeps beacon latency low.
	ForceNetUpdate();
}

void AArtemisPlayerState::OnRep_CueState()
{
	OnCueStateChanged.Broadcast(CueState);
}

void AArtemisPlayerState::EmitEvent(EGameEventType Event)
{
	if (UPlayerActionProvider* Provider = GetProvider())
	{
		Provider->ServerReportCueEvent(UPID, CueState, Event);
	}
}

UPlayerActionProvider* AArtemisPlayerState::GetProvider() const
{
	return GetWorld() ? GetWorld()->GetSubsystem<UPlayerActionProvider>() : nullptr;
}

// ---- Lookups ----

AArtemisPlayerState* AArtemisPlayerState::FindByUPID(const UWorld* World, const FString& InUPID)
{
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	if (!GS || InUPID.IsEmpty())
	{
		return nullptr;
	}
	for (APlayerState* PS : GS->PlayerArray)
	{
		AArtemisPlayerState* APS = Cast<AArtemisPlayerState>(PS);
		if (APS && APS->UPID == InUPID)
		{
			return APS;
		}
	}
	return nullptr;
}

AArtemisPlayerState* AArtemisPlayerState::FindForActor(const UWorld* World, const AActor* Actor)
{
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	if (!GS || !Actor)
	{
		return nullptr;
	}
	for (APlayerState* PS : GS->PlayerArray)
	{
		AArtemisPlayerState* APS = Cast<AArtemisPlayerState>(PS);
		if (!APS)
		{
			continue;
		}
		const AActor* VR    = APS->VRChar.Get();
		const AActor* AR    = APS->ARPawn.Get();
		const AActor* Rover = APS->MasterRover.Get();
		if ((VR && VR == Actor) || (AR && AR == Actor) || (Rover && Rover == Actor))
		{
			return APS;
		}
	}
	return nullptr;
}
