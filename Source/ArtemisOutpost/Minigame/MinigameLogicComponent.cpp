// Fill out your copyright notice in the Description page of Project Settings.

#include "ArtemisOutpost/Minigame/MinigameLogicComponent.h"
#include "ArtemisOutpost/Connection/ConnectionComponent.h"
#include "Net/UnrealNetwork.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

UMinigameLogicComponent::UMinigameLogicComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UMinigameLogicComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UMinigameLogicComponent, State);
}

void UMinigameLogicComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	// The connection owns join/leave; the logic only reacts and gates.
	if (UConnectionComponent* Connection = GetConnection())
	{
		Connection->CanJoinPredicate.BindUObject(this, &UMinigameLogicComponent::HandleCanJoin);
		Connection->OnParticipantJoined.AddUObject(this, &UMinigameLogicComponent::HandleParticipantJoined);
		Connection->OnParticipantLeft.AddUObject(this, &UMinigameLogicComponent::HandleParticipantLeft);
	}
}

UConnectionComponent* UMinigameLogicComponent::GetConnection() const
{
	if (!CachedConnection && GetOwner())
	{
		const_cast<UMinigameLogicComponent*>(this)->CachedConnection = GetOwner()->FindComponentByClass<UConnectionComponent>();
	}
	return CachedConnection;
}

EMinigameState UMinigameLogicComponent::GetState() const
{
	return State;
}

void UMinigameLogicComponent::ServerHandleInput(const FString& UPID, const FMinigameInput& Input)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || State != EMinigameState::Active)
	{
		return;
	}

	const UConnectionComponent* Connection = GetConnection();
	if (!Connection || !Connection->IsParticipant(UPID))
	{
		return;
	}

	ApplyInput(UPID, Input);
	PublishSnapshot();
}

bool UMinigameLogicComponent::HandleCanJoin(const FString& UPID, FText& OutReason)
{
	// Additional joiners are always fine (slot rules already checked by the connection). Only
	// the first joiner, which starts the task, must satisfy the game preconditions.
	return State != EMinigameState::Idle ? true : CanStart(UPID, OutReason);
}

void UMinigameLogicComponent::HandleParticipantJoined(const FString& UPID)
{
	if (State == EMinigameState::Idle)
	{
		SetState(EMinigameState::Active);
		OnStart();
	}
	
	OnParticipantJoined(UPID);
	
	PublishSnapshot();
}

void UMinigameLogicComponent::HandleParticipantLeft(const FString& UPID)
{
	OnParticipantLeft(UPID);

	const UConnectionComponent* Connection = GetConnection();
	if (Connection && Connection->GetParticipantCount() == 0 && State != EMinigameState::Completed)
	{
		OnAbort();
		SetState(EMinigameState::Idle);
	}
	
	PublishSnapshot();
}

bool UMinigameLogicComponent::CanStart(const FString& UPID, FText& OutReason) const
{
	return true;
}

void UMinigameLogicComponent::OnStart()
{
}

void UMinigameLogicComponent::OnParticipantJoined(const FString& UPID)
{
}

void UMinigameLogicComponent::OnParticipantLeft(const FString& UPID)
{
}

void UMinigameLogicComponent::ApplyInput(const FString& UPID, const FMinigameInput& Input)
{
}

void UMinigameLogicComponent::OnComplete()
{
	SetState(EMinigameState::Completed);
}

void UMinigameLogicComponent::OnAbort()
{
}

void UMinigameLogicComponent::SetState(EMinigameState NewState)
{
	if (State == NewState)
	{
		return;
	}
	State = NewState;
	HandleStateChanged();
}

void UMinigameLogicComponent::OnRep_State()
{
	HandleStateChanged();
}

void UMinigameLogicComponent::HandleStateChanged()
{
	OnStateChanged.Broadcast();
	
	PublishSnapshot();
}

void UMinigameLogicComponent::PublishSnapshot()
{
	if (!OnSnapshotChanged.IsBound())
	{
		return;
	}

	FString Json;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	FJsonSerializer::Serialize(BuildSnapshot(), Writer);
	OnSnapshotChanged.Broadcast(Json);
}

TSharedRef<FJsonObject> UMinigameLogicComponent::BuildSnapshot() const
{
	TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("actor"), GetOwner() ? GetOwner()->GetName() : TEXT(""));
	Obj->SetNumberField(TEXT("state"), static_cast<int32>(State));

	if (const UConnectionComponent* Connection = GetConnection())
	{
		TArray<TSharedPtr<FJsonValue>> Participants;
		for (const FString& UPID : Connection->GetParticipantUPIDs())
		{
			Participants.Add(MakeShared<FJsonValueString>(UPID));
		}
		Obj->SetArrayField(TEXT("participants"), Participants);
	}
	return Obj;
}
