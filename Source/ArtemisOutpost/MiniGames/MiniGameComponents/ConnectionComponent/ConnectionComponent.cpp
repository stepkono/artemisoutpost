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
	
	if (GameMode)
	{
		//const FArtemisPlayer* ArtemisPLayer = GameMode->GetPlayersInGame().Find(UPID);
		//ArtemisPLayer->PawnController->SetIsInGame(true); 
	}
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
	if (GameMode)
	{
		const FArtemisPlayer* ArtemisPLayer = GameMode->GetPlayersInGame().Find(UPID);
		ArtemisPLayer->PawnController->SetIsInGame(false); 
	}
	OnParticipantLeft.Broadcast(UPID);
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
