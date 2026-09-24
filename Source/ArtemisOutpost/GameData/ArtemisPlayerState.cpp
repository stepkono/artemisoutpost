// Fill out your copyright notice in the Description page of Project Settings.

#include "ArtemisPlayerState.h"

#include "GameFramework/GameStateBase.h"
#include "Engine/Engine.h"
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
	DOREPLIFETIME_CONDITION(AArtemisPlayerState, TrackerPose, COND_SkipOwner);
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

// ---- Tracker pose ----

void AArtemisPlayerState::ServerSetTrackerPose(const FTransform& Head, const FTransform& Left, const FTransform& Right, bool bLeftTracked, bool bRightTracked)
{
	if (!HasAuthority())
	{
		if (!bWarnedTrackerPoseNoAuthority)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Cues] ServerSetTrackerPose on %s (UPID '%s') called WITHOUT authority, ignored. Call it from a Run on Server event."),
				*GetName(), *UPID);
			bWarnedTrackerPoseNoAuthority = true;
		}
		return;
	}

	TrackerPose.Head          = Head;
	TrackerPose.Left          = Left;
	TrackerPose.Right         = Right;
	TrackerPose.bLeftTracked  = bLeftTracked;
	TrackerPose.bRightTracked = bRightTracked;

	const UWorld* World = GetWorld();
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	TrackerPose.ServerWriteTime = GS ? GS->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0);
	++TrackerPoseWritesSinceLog;

	const double Now = World ? World->GetTimeSeconds() : 0.0;

	// Writing into a spectator (the listen-server host) means the sender resolved the wrong PlayerState, typically
	// GameplayStatics "Get Player State (index 0)" instead of the pawn's own PlayerState.
	if ((IsSpectator() || IsOnlyASpectator() || (!VRChar && !ARPawn)) && Now - LastTrackerPoseSanityWarnTime >= 5.0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cues] SERVER tracker pose written into %s (PlayerId=%d, UPID '%s') which is a spectator / has no pawns. The sender resolved the WRONG PlayerState."),
			*GetName(), GetPlayerId(), *UPID);
		LastTrackerPoseSanityWarnTime = Now;
	}

	// Sanity check on the sender's data: the values are fractions of the table edge, so a head more than ~10 edges
	// from the table center means that sender's anchors are not at the physical table (or it sent world poses).
	const double HeadFraction = Head.GetLocation().Size();
	if (HeadFraction > 10.0 && Now - LastTrackerPoseSanityWarnTime >= 5.0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cues] SERVER '%s' (%s, PlayerId=%d): head is %.0f table edges from the table center. The sender's anchors are not at the physical table, or it sent unconverted world poses. H=%s"),
			*UPID, *GetName(), GetPlayerId(), HeadFraction, *Head.GetLocation().ToString());
		LastTrackerPoseSanityWarnTime = Now;
	}

	// Two bit-identical controller poses are the untracked fallback (both sitting on the tracking origin).
	if (Left.Equals(Right, 0.0) && (bLeftTracked || bRightTracked) && Now - LastTrackerPoseSanityWarnTime >= 5.0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cues] SERVER '%s' (PlayerId=%d): left and right controller poses are identical but reported tracked (L=%d R=%d). The tracked flags do not reflect the device state."),
			*UPID, GetPlayerId(), bLeftTracked ? 1 : 0, bRightTracked ? 1 : 0);
		LastTrackerPoseSanityWarnTime = Now;
	}

	if (!bLoggedFirstTrackerPose)
	{
		UE_LOG(LogTemp, Log, TEXT("[Cues] SERVER '%s' (%s) first tracker pose | H=%s L=%s (tracked=%d) R=%s (tracked=%d)"),
			*UPID, *GetName(), *Head.GetLocation().ToString(), *Left.GetLocation().ToString(), bLeftTracked ? 1 : 0,
			*Right.GetLocation().ToString(), bRightTracked ? 1 : 0);
		bLoggedFirstTrackerPose = true;
		LastTrackerPoseLogTime = Now;
		TrackerPoseWritesSinceLog = 0;
	}
	else if (Now - LastTrackerPoseLogTime >= TrackerPoseLogInterval)
	{
		// Values are anchor-frame fractions: locations are roughly within [-1, 1] near the table. Values in the
		// thousands mean world coordinates were sent without GetRelativeToAnchorsFrame.
		UE_LOG(LogTemp, Log, TEXT("[Cues] SERVER '%s' tracker pose | %d writes in %.1fs | H=%s rot=%s | L=%s rot=%s tracked=%d | R=%s rot=%s tracked=%d | t=%.2f"),
			*UPID, TrackerPoseWritesSinceLog, Now - LastTrackerPoseLogTime,
			*Head.GetLocation().ToString(),  *Head.Rotator().ToString(),
			*Left.GetLocation().ToString(),  *Left.Rotator().ToString(),  bLeftTracked ? 1 : 0,
			*Right.GetLocation().ToString(), *Right.Rotator().ToString(), bRightTracked ? 1 : 0,
			TrackerPose.ServerWriteTime);
		LastTrackerPoseLogTime = Now;
		TrackerPoseWritesSinceLog = 0;
	}

	// Push at send rate instead of waiting for the next NetUpdateFrequency slot.
	ForceNetUpdate();
}

void AArtemisPlayerState::OnRep_TrackerPose()
{
	++TrackerPoseRepsSinceLog;

	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	if (Now - LastTrackerPoseRepLogTime < TrackerPoseLogInterval)
	{
		return;
	}

	// Age uses the server clock estimate of this client. A large age right after the first update can mean the
	// server time has not synced yet (AGameStateBase replicates it periodically).
	UE_LOG(LogTemp, Log, TEXT("[Cues] CLIENT received '%s' tracker pose | %d updates in %.1fs | age=%.2fs | H=%s | L=%s tracked=%d | R=%s tracked=%d"),
		*UPID, TrackerPoseRepsSinceLog, Now - LastTrackerPoseRepLogTime, GetTrackerPoseAgeSeconds(),
		*TrackerPose.Head.GetLocation().ToString(),
		*TrackerPose.Left.GetLocation().ToString(),  TrackerPose.bLeftTracked  ? 1 : 0,
		*TrackerPose.Right.GetLocation().ToString(), TrackerPose.bRightTracked ? 1 : 0);

	LastTrackerPoseRepLogTime = Now;
	TrackerPoseRepsSinceLog = 0;
}

double AArtemisPlayerState::GetTrackerPoseAgeSeconds() const
{
	return TrackerPose.GetAgeSeconds(GetWorld());
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

AArtemisPlayerState* AArtemisPlayerState::FindPlayerStateForActor(const UObject* WorldContextObject, const AActor* Actor)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	return FindForActor(World, Actor);
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
