// Fill out your copyright notice in the Description page of Project Settings.

#include "ConnectionComponent.h"

#include "ArtemisOutpost/MiniGames/General/MinigameTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

UConnectionComponent::UConnectionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UConnectionComponent::BeginPlay()
{
	Super::BeginPlay();
	
	if (AServerGameMode* ServerGameMode = Cast<AServerGameMode>(UGameplayStatics::GetGameMode(GetWorld())))
	{
		GameMode = ServerGameMode;	
	}
}

void UConnectionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UConnectionComponent, MaxSlots);
	DOREPLIFETIME(UConnectionComponent, ActiveSlots);
}

void UConnectionComponent::ServerRequestJoin(const FString& UPID)
{
	const FString OwnerName = GetOwner() ? GetOwner()->GetName() : TEXT("<no owner>");

	if (!GetOwner())
	{
		UE_LOG(LogMinigame, Error, TEXT("[Connect] REFUSED: connection component has no owning actor."));
		return;
	}
	if (!GetOwner()->HasAuthority())
	{
		UE_LOG(LogMinigame, Error, TEXT("[Connect] %s: REFUSED for '%s' -> no authority. ServerRequestJoin ran on a client, so the Server RPC did not route."),
			*OwnerName, *UPID);
		return;
	}
	if (UPID.IsEmpty())
	{
		// The server-side UPID comes from the ?UPID= login option (AServerGameMode::InitNewPlayer).
		// Empty means this player's controller never got one, so no join can ever be attributed.
		UE_LOG(LogMinigame, Error, TEXT("[Connect] %s: REFUSED -> the requesting player's UPID is EMPTY. Check APawnController::SetUPID / the ?UPID= login option."),
			*OwnerName);
		return;
	}

	if (IsParticipant(UPID))
	{
		UE_LOG(LogMinigame, Warning, TEXT("[Connect] %s: REFUSED for '%s' -> already a participant (slot %d)."),
			*OwnerName, *UPID, GetSlotIndexFor(UPID));
		return;
	}
	if (GetFreeSlotCount() <= 0)
	{
		// MaxSlots defaults to 1. A two-person coupled task needs it set to 2 on the actor BP.
		UE_LOG(LogMinigame, Warning, TEXT("[Connect] %s: REFUSED for '%s' -> no free slot (%d/%d occupied). Raise MaxSlots on the ConnectionComponent if more players should fit."),
			*OwnerName, *UPID, ActiveSlots.Num(), MaxSlots);
		return;
	}

	// Let the minigame logic veto on its own preconditions before we commit a slot.
	if (CanJoinPredicate.IsBound())
	{
		FText Reason;
		if (!CanJoinPredicate.Execute(UPID, Reason))
		{
			UE_LOG(LogMinigame, Warning, TEXT("[Connect] %s: REFUSED for '%s' -> the game vetoed the join: %s"),
				*OwnerName, *UPID,
				Reason.IsEmpty() ? TEXT("(no reason given)") : *Reason.ToString());
			return;
		}
	}
	else
	{
		UE_LOG(LogMinigame, Warning, TEXT("[Connect] %s: no CanJoinPredicate bound -> game preconditions are NOT being checked. Expected the owning AMinigameActor to bind it in BeginPlay (server only)."),
			*OwnerName);
	}

	FConnectionSlot NewSlot;
	NewSlot.OwnerUPID = UPID;
	ActiveSlots.Add(NewSlot);
	
	// Still commented out as you left it: nothing reads bIsInGame yet, so the join side was never
	// enabled. Uncomment when something needs it — the helper is now safe to call from both sides.
	// SetParticipantInGame(UPID, true);

	OnParticipantJoined.Broadcast(UPID);
}

void UConnectionComponent::ServerRequestLeave(const FString& UPID)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	const int32 Index = GetSlotIndexFor(UPID);
	if (Index == INDEX_NONE)
	{
		UE_LOG(LogMinigame, Warning, TEXT("[Disconnect] %s: '%s' asked to leave but holds no slot."),
			*GetOwner()->GetName(), *UPID);
		return;
	}

	UE_LOG(LogMinigame, Log, TEXT("[Disconnect] %s: '%s' LEFT slot %d."), *GetOwner()->GetName(), *UPID, Index);

	ActiveSlots.RemoveAt(Index);
	SetParticipantInGame(UPID, false);
	OnParticipantLeft.Broadcast(UPID);
}

void UConnectionComponent::SetParticipantInGame(const FString& UPID, bool bIsPlayingMinigame) const
{
	if (!GameMode)
	{
		return;
	}

	// GetPlayersInGame() returns the map BY VALUE, so the result must be held in a named local.
	// Calling .Find() straight on the call expression yields a pointer into a temporary that dies
	// at the end of the statement, and dereferencing it is undefined behaviour even when the key
	// exists. That is what crashed the server on the first ever leave.
	const TMap<FString, FArtemisPlayer> Players = GameMode->GetPlayersInGame();

	const FArtemisPlayer* Player = Players.Find(UPID);
	if (!Player)
	{
		UE_LOG(LogMinigame, Warning, TEXT("[Connect] '%s' is not in AServerGameMode::PlayersInGame -> cannot set IsInGame=%s."),
			*UPID, bIsPlayingMinigame ? TEXT("true") : TEXT("false"));
		return;
	}

	if (!Player->PawnController)
	{
		UE_LOG(LogMinigame, Warning, TEXT("[Connect] '%s' has no PawnController in the registry -> cannot set IsInGame=%s."),
			*UPID, bIsPlayingMinigame ? TEXT("true") : TEXT("false"));
		return;
	}

	Player->PawnController->SetIsPlayingMiniGame(bIsPlayingMinigame);
}

int32 UConnectionComponent::GetMaxSlots() const
{
	return MaxSlots;
}

int32 UConnectionComponent::GetFreeSlotCount() const
{
	return MaxSlots - ActiveSlots.Num();
}

int32 UConnectionComponent::GetParticipantCount() const
{
	return ActiveSlots.Num();
}

bool UConnectionComponent::IsParticipant(const FString& UPID) const
{
	return GetSlotIndexFor(UPID) != INDEX_NONE;
}

TArray<FString> UConnectionComponent::GetParticipantUPIDs() const
{
	TArray<FString> Result;
	Result.Reserve(ActiveSlots.Num());
	for (const FConnectionSlot& Slot : ActiveSlots)
	{
		Result.Add(Slot.OwnerUPID);
	}
	return Result;
}

int32 UConnectionComponent::GetSlotIndexFor(const FString& UPID) const
{
	for (int32 i = 0; i < ActiveSlots.Num(); ++i)
	{
		if (ActiveSlots[i].OwnerUPID == UPID)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

void UConnectionComponent::OnRep_ActiveSlots()
{
	OnSlotsChanged.Broadcast();
}
