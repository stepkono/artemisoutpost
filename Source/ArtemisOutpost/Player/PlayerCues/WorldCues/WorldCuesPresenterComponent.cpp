// Fill out your copyright notice in the Description page of Project Settings.

#include "WorldCuesPresenterComponent.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "ArtemisOutpost/GameData/ArtemisPlayerState.h"
#include "ArtemisOutpost/GameData/ArtemisGameState.h"
#include "WorldCue.h"

UWorldCuesPresenterComponent::UWorldCuesPresenterComponent()
{
	// Always ticks: the Blueprint child's Event Tick decides which cues are on, and inactive cues cannot wake up themselves.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UWorldCuesPresenterComponent::BeginPlay()
{
	Super::BeginPlay();

	// Cues are a client-side view. The listen-server host is a spectator and has no anchors,
	// so presenting there only spawns cues nobody sees and polls an anchors frame that never exists.
	const ENetMode NetMode = GetNetMode();
	if (NetMode == NM_ListenServer || NetMode == NM_DedicatedServer)
	{
		SetComponentTickEnabled(false);
	}
}

void UWorldCuesPresenterComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Spawned actors do not die with their spawner.
	DestroyAllCues();
	Super::EndPlay(EndPlayReason);
}

void UWorldCuesPresenterComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	// Runs the Blueprint child's Event Tick (spawning and switching) first, so the summary reflects this frame.
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bLogSummary)
	{
		return;
	}
	SummaryLogTimer += DeltaTime;
	if (SummaryLogTimer < SummaryLogInterval)
	{
		return;
	}
	SummaryLogTimer = 0.0f;
	LogSummary();
}

AArtemisPlayerState* UWorldCuesPresenterComponent::GetOwnerPawnPlayerState() const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	return OwnerPawn ? OwnerPawn->GetPlayerState<AArtemisPlayerState>() : nullptr;
}

bool UWorldCuesPresenterComponent::GetOwnerPlayerIdentity(FCuePlayerIdentity& OutIdentity) const
{
	OutIdentity = FCuePlayerIdentity();

	const AArtemisPlayerState* OwnerPlayerState = GetOwnerPawnPlayerState();
	if (!OwnerPlayerState)
	{
		return false;
	}
	OutIdentity.UPID         = OwnerPlayerState->GetUPID();
	OutIdentity.PlayerNumber = OwnerPlayerState->GetPlayerNumber();
	if (OutIdentity.UPID.IsEmpty())
	{
		return false;
	}

	// The roster entry carries the colour AServerGameMode assigned. Matched by UPID: survives reconnects, unlike the
	// PlayerState pointer.
	const UWorld* World = GetWorld();
	const AArtemisGameState* GameState = World ? World->GetGameState<AArtemisGameState>() : nullptr;
	if (!GameState)
	{
		return false;
	}
	for (const FArtemisPlayer& Record : GameState->PlayersRoster)
	{
		if (Record.UPID == OutIdentity.UPID)
		{
			OutIdentity.Color = Record.PlayerColor;
			return true;
		}
	}
	return false;
}

AArtemisPlayerState* UWorldCuesPresenterComponent::GetLocalViewerPlayerState() const
{
	const UWorld* World = GetWorld();
	const APlayerController* LocalPC = World ? World->GetFirstPlayerController() : nullptr;
	return LocalPC ? LocalPC->GetPlayerState<AArtemisPlayerState>() : nullptr;
}

bool UWorldCuesPresenterComponent::GetLocalViewerContext(ERVMode& OutContext) const
{
	const AArtemisPlayerState* ViewerPlayerState = GetLocalViewerPlayerState();
	if (!ViewerPlayerState)
	{
		return false;
	}
	OutContext = ViewerPlayerState->GetCueState().Context;
	return true;
}

AWorldCue* UWorldCuesPresenterComponent::SpawnCue(TSubclassOf<AWorldCue> CueClass)
{
	UWorld* World = GetWorld();
	if (!World || !CueClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WorldCues] SpawnCue on %s: no %s, nothing spawned."),
			*GetNameSafe(GetOwner()), World ? TEXT("cue class") : TEXT("world"));
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.Owner = GetOwner();
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AWorldCue* Cue = World->SpawnActor<AWorldCue>(CueClass, FTransform::Identity, Params);
	if (!Cue)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WorldCues] SpawnCue on %s: spawning %s failed."), *GetNameSafe(GetOwner()), *GetNameSafe(CueClass.Get()));
		return nullptr;
	}

	Cues.Add(Cue);

	UE_LOG(LogTemp, Log, TEXT("[WorldCues] Spawned %s on %s | cue context=%s"),
		*Cue->GetName(), *GetNameSafe(GetOwner()), *UEnum::GetValueAsString(Cue->GetLocalViewerCueContext()));
	return Cue;
}

void UWorldCuesPresenterComponent::WritePoseToCues(const FTrackerPose& Pose)
{
	for (AWorldCue* Cue : Cues)
	{
		if (IsValid(Cue))
		{
			Cue->WritePose(Pose);
		}
	}
}

void UWorldCuesPresenterComponent::WritePlayerCueStateToCues(const FPlayerCueState& CueState)
{
	for (AWorldCue* Cue : Cues)
	{
		if (IsValid(Cue))
		{
			Cue->WritePlayerCueState(CueState);
		}
	}
}

void UWorldCuesPresenterComponent::WritePlayerIdentityToCues(const FCuePlayerIdentity& Identity)
{
	for (AWorldCue* Cue : Cues)
	{
		if (IsValid(Cue))
		{
			Cue->WritePlayerIdentity(Identity);
		}
	}
}

void UWorldCuesPresenterComponent::WriteViewerTrackersToCues(const FCueLocalViewerTrackers& Trackers)
{
	for (AWorldCue* Cue : Cues)
	{
		if (IsValid(Cue))
		{
			Cue->WriteViewerTrackers(Trackers);
		}
	}
}

void UWorldCuesPresenterComponent::ToggleCuesOfContext(ERVMode CueContext, bool bActivate)
{
	RemoveInvalidCues();
	// Copy: OnCueActivationChanged runs Blueprint code that could spawn or destroy cues.
	const TArray<TObjectPtr<AWorldCue>> Snapshot = Cues;
	for (AWorldCue* Cue : Snapshot)
	{
		if (IsValid(Cue) && Cue->GetLocalViewerCueContext() == CueContext)
		{
			Cue->ToggleCueActivation(bActivate);
		}
	}
}

void UWorldCuesPresenterComponent::ToggleAllCues(bool bActivate)
{
	RemoveInvalidCues();
	const TArray<TObjectPtr<AWorldCue>> Snapshot = Cues;
	for (AWorldCue* Cue : Snapshot)
	{
		if (IsValid(Cue))
		{
			Cue->ToggleCueActivation(bActivate);
		}
	}
}

void UWorldCuesPresenterComponent::DestroyAllCues()
{
	int32 Destroyed = 0;
	for (AWorldCue* Cue : Cues)
	{
		if (IsValid(Cue))
		{
			Cue->Destroy();
			++Destroyed;
		}
	}
	Cues.Empty();

	if (Destroyed > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[WorldCues] Destroyed %d cue(s) of %s"), Destroyed, *GetNameSafe(GetOwner()));
	}
}

void UWorldCuesPresenterComponent::RemoveInvalidCues()
{
	Cues.RemoveAll([](const TObjectPtr<AWorldCue>& Cue) { return !IsValid(Cue); });
}

void UWorldCuesPresenterComponent::LogSummary() const
{
	const AArtemisPlayerState* OwnerPlayerState = GetOwnerPawnPlayerState();
	if (!OwnerPlayerState && Cues.Num() == 0)
	{
		// Parked pawn without cues: nothing to report, and every client has several of these.
		return;
	}

	ERVMode ViewerContext = ERVMode::AR;
	const bool bHasViewerContext = GetLocalViewerContext(ViewerContext);
	const AArtemisPlayerState* ViewerPlayerState = GetLocalViewerPlayerState();

	FString CueStates;
	for (const AWorldCue* Cue : Cues)
	{
		if (!IsValid(Cue))
		{
			CueStates += TEXT(" [invalid]");
			continue;
		}
		CueStates += FString::Printf(TEXT(" [%s ctx=%s active=%d]"),
			*Cue->GetClass()->GetName(), *UEnum::GetValueAsString(Cue->GetLocalViewerCueContext()), Cue->IsCueActive() ? 1 : 0);
	}

	UE_LOG(LogTemp, Log, TEXT("[WorldCues] Presenter %s (%s) | owner possessed=%d UPID='%s' local=%d | viewer ctx=%s | cues=%d%s"),
		*GetNameSafe(GetOwner()), *GetClass()->GetName(),
		OwnerPlayerState ? 1 : 0,
		OwnerPlayerState ? *OwnerPlayerState->GetUPID() : TEXT(""),
		(OwnerPlayerState && OwnerPlayerState == ViewerPlayerState) ? 1 : 0,
		bHasViewerContext ? *UEnum::GetValueAsString(ViewerContext) : TEXT("unknown"),
		Cues.Num(), *CueStates);
}
