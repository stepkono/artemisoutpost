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
	if (!GetOwner() || !GetOwner()->HasAuthority() || UPID.IsEmpty())
	{
		return;
	}

	if (IsParticipant(UPID) || GetFreeSlotCount() <= 0)
	{
		return;
	}

	// Let the minigame logic veto on its own preconditions before we commit a slot.
	if (CanJoinPredicate.IsBound())
	{
		FText Reason;
		if (!CanJoinPredicate.Execute(UPID, Reason))
		{
			return;
		}
	}

	FConnectionSlot NewSlot;
	NewSlot.OwnerUPID = UPID;
	ActiveSlots.Add(NewSlot);
	
	if (GameMode)
	{
		const FArtemisPlayer* ArtemisPLayer = GameMode->GetPlayersInGame().Find(UPID);
		ArtemisPLayer->PawnController->SetIsInGame(true); 
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
		return;
	}

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
