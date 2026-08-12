// Fill out your copyright notice in the Description page of Project Settings.

#include "MinigameLogicComponent.h"

#include "ArtemisOutpost/Minigame/General/GameInstance/MinigameActor.h"
#include "ArtemisOutpost/Minigame/General/MinigameTypes.h"
#include "ArtemisOutpost/Minigame/Connection/ConnectionComponent.h"
#include "ArtemisOutpost/Networking/ClientServerConnection/NetUtils.h"
#include "ArtemisOutpost/Player/PawnController.h"
#include "ArtemisOutpost/Player/MiniGameInteraction/MinigamePlayerController.h"
#include "Kismet/GameplayStatics.h"
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

	UConnectionComponent* Connection = GetConnection();
	if (!Connection)
	{
		return;
	}

	// Clients only: the local player's screen View reacts to slot changes.
	if (ArtemisNet::IsClientContext(GetNetMode()))
	{
		Connection->OnSlotsChanged.AddDynamic(this, &UMinigameLogicComponent::RefreshLocalUI);	
	}

	// Server only: authoritative join gating + game reactions.
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		Connection->CanJoinPredicate.BindUObject(this, &UMinigameLogicComponent::HandleCanJoin);
		Connection->OnParticipantJoined.AddUObject(this, &UMinigameLogicComponent::HandleParticipantJoined);
		Connection->OnParticipantLeft.AddUObject(this, &UMinigameLogicComponent::HandleParticipantLeft);
	}
}

TSubclassOf<UMiniGameUI> UMinigameLogicComponent::GetMiniGameUIClass() const
{
	return MiniGameUIClass;
}

void UMinigameLogicComponent::RefreshLocalUI()
{
	APawnController* PC = Cast<APawnController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
	if (!PC)
	{
		return;
	}

	UMinigamePlayerController* Controller = PC->GetMinigamePlayerController();
	if (!Controller)
	{
		return;
	}

	const UConnectionComponent* Connection = GetConnection();
	const bool bShouldOpen = Connection && Connection->IsParticipant(PC->GetPlayerUPID());

	if (bShouldOpen && !bLocalUIOpen)
	{
		Controller->OpenUI(Cast<AMinigameActor>(GetOwner()));
		bLocalUIOpen = true;
	}
	else if (!bShouldOpen && bLocalUIOpen)
	{
		Controller->CloseUI();
		bLocalUIOpen = false;
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

	// State changes (Active on start, Completed/Idle on finish/abort) also drive the local View.
	RefreshLocalUI();
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
