// Fill out your copyright notice in the Description page of Project Settings.

#include "MinigameActor.h"

#include "ArtemisOutpost/MiniGames/MiniGameComponents/ConnectionComponent/ConnectionComponent.h"
#include "ArtemisOutpost/MiniGames/MiniGameComponents/PuppetManagerComponent/MinigamePuppetManagerComponent.h"
#include "ArtemisOutpost/Moon/MoonBuildings/MoonBuildingsManager.h"
#include "ArtemisOutpost/Networking/ClientServerConnection/NetUtils.h"
#include "ArtemisOutpost/Player/PawnVR/ACharVR.h"
#include "ArtemisOutpost/Player/PlayerController/PawnController.h"
#include "ArtemisOutpost/Player/PlayerController/PlayerControllerComponents/MiniGameInteraction/MinigamePlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

AMinigameActor::AMinigameActor()
{
	PrimaryActorTick.bCanEverTick = true;

	// Few minigame actors exist and both nearby VR manipulators and the (possibly distant) AR
	// proxy need the state, so keep them always relevant rather than distance-culled.
	bReplicates = true;
	bAlwaysRelevant = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneRoot"));
	RootComponent = Root;

	GameConnection = CreateDefaultSubobject<UConnectionComponent>(TEXT("Connection"));
	PuppetManager  = CreateDefaultSubobject<UMinigamePuppetManagerComponent>(TEXT("PuppetManager"));
}

void AMinigameActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMinigameActor, State);
	DOREPLIFETIME(AMinigameActor, MGID);
}

void AMinigameActor::BeginPlay()
{
	Super::BeginPlay();

	// The concrete subclass adds the prompt component; resolve it once.
	ConnectionUIHolder = FindComponentByClass<UMiniGameConnectionUIComponent>();

	if (!GameConnection)
	{
		UE_LOG(LogTemp, Error, TEXT("MiniGameActor: Missing GameConnection component. Aborting further initialization."));
		return;
	}
	
	if (ArtemisNet::IsClientContext(GetNetMode()))
	{
		GameConnection->OnSlotsChanged.AddDynamic(this, &AMinigameActor::RefreshLocalUI);

		if (PuppetManager)
		{
			PuppetManager->CreateARPuppet();
			SyncPuppet();
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("MiniGameActor: PuppetManager is null."))
		}
	}

	// Server only: identity, registry, authoritative join gating + game reactions.
	if (HasAuthority())
	{
		MGID = FGuid::NewGuid();
		RegisterWithBuildingsManager();

		GameConnection->CanJoinPredicate.BindUObject(this, &AMinigameActor::ServerHandleCanJoin);
		GameConnection->OnParticipantJoined.AddUObject(this, &AMinigameActor::ServerHandleParticipantJoined);
		GameConnection->OnParticipantLeft.AddUObject(this, &AMinigameActor::ServerHandleParticipantLeft);
	}
}

FInstancedStruct AMinigameActor::MakeInitialTypeData() const
{
	return FInstancedStruct();
}

void AMinigameActor::RegisterWithBuildingsManager()
{
	UMoonBuildingsManager* Manager = GetWorld() ? GetWorld()->GetSubsystem<UMoonBuildingsManager>() : nullptr;
	if (!Manager)
	{
		UE_LOG(LogTemp, Error, TEXT("MiniGameActor: MoonBuildingsManager subsystem not found; not registered."));
		return;
	}

	FMiniGameRecord Record;
	Record.MGID          = MGID;
	Record.BuildLocation = GetActorLocation();
	Record.State         = State;
	Record.Type          = GetBuildingType();
	Record.MiniGameData  = MakeInitialTypeData();
	// BuiltByUPID: the placing player is not tracked here yet; fill in once the build tool passes it.

	Manager->RegisterMinigame(Record);
}

void AMinigameActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (ConnectionUIHolder)
	{
		ConnectionUIHolder->SetShowConnectionUI(IsPlayerNear() && CanLocalPlayerConnect());
	}
}

UConnectionComponent* AMinigameActor::GetConnectionComponent() const
{
	return GameConnection;
}

EMinigameState AMinigameActor::GetState() const
{
	return State;
}

TSubclassOf<UMiniGameUI> AMinigameActor::GetMiniGameUIClass() const
{
	return MiniGameUIClass;
}

// ---- Server-authoritative mechanics ----

void AMinigameActor::ServerHandleInput(const FString& UPID, const FMinigameInput& Input)
{
	if (!HasAuthority() || State != EMinigameState::Active)
	{
		return;
	}

	if (!GameConnection || !GameConnection->IsParticipant(UPID))
	{
		return;
	}

	ApplyInput(UPID, Input);
	PublishSnapshot();
}

bool AMinigameActor::ServerHandleCanJoin(const FString& UPID, FText& OutReason)
{
	// Additional joiners are always fine (slot rules already checked by the connection). Only the
	// first joiner, which starts the task, must satisfy the game preconditions.
	if (State != EMinigameState::Idle)
	{
		return true;
	}
	return CanStart(UPID, OutReason);
}

void AMinigameActor::ServerHandleParticipantJoined(const FString& UPID)
{
	if (State == EMinigameState::Idle)
	{
		SetState(EMinigameState::Active);
		OnStart();
	}
	
	ActivePlayers.Add(UPID); 

	OnParticipantJoined(UPID);
	PublishSnapshot();
}

void AMinigameActor::ServerHandleParticipantLeft(const FString& UPID)
{
	OnParticipantLeft(UPID);

	ActivePlayers.Remove(UPID);
		
	if (GameConnection && GameConnection->GetParticipantCount() == 0 && State != EMinigameState::Completed)
	{
		OnAbort();
		SetState(EMinigameState::Idle);
	}

	PublishSnapshot();
}

// ---- Rules + mechanics hooks (base defaults) ----

bool AMinigameActor::CanStart(const FString& UPID, FText& OutReason) const
{
	return true;
}

void AMinigameActor::OnStart()
{
}

void AMinigameActor::OnComplete()
{
	SetState(EMinigameState::Completed);
}

void AMinigameActor::OnAbort()
{
}

void AMinigameActor::ApplyInput(const FString& UPID, const FMinigameInput& Input)
{
}

void AMinigameActor::OnParticipantJoined(const FString& UPID)
{
}

void AMinigameActor::OnParticipantLeft(const FString& UPID)
{
}

// ---- State machine ----

void AMinigameActor::SetState(EMinigameState NewState)
{
	if (State == NewState)
	{
		return;
	}
	State = NewState;
	HandleStateChanged();
}

void AMinigameActor::OnRep_State()
{
	HandleStateChanged();
}

void AMinigameActor::HandleStateChanged()
{
	OnStateChanged.Broadcast();
	// TODO: check what should be server side and what should be client side only
	
	if (HasAuthority())
	{
		PublishSnapshot();

		if (UMoonBuildingsManager* Manager = GetWorld() ? GetWorld()->GetSubsystem<UMoonBuildingsManager>() : nullptr)
		{
			Manager->UpdateState(MGID, State);
		}
	}

	// State changes (Active on start, Completed/Idle on finish/abort) also drive the local View...
	RefreshLocalUI();
	
	// ...and the AR puppet (no-op where no puppet exists).
	if (PuppetManager)
	{
		PuppetManager->PushState(State);
	}
}

// ---- Local screen-View lifecycle ----

void AMinigameActor::RefreshLocalUI()
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

	const bool bShouldOpen = GameConnection && GameConnection->IsParticipant(PC->GetPlayerUPID());

	if (bShouldOpen && !bLocalUIOpen)
	{
		Controller->OpenUI(this);
		bLocalUIOpen = true;
	}
	else if (!bShouldOpen && bLocalUIOpen)
	{
		Controller->CloseUI();
		bLocalUIOpen = false;
	}
}

// ---- Snapshot ----

void AMinigameActor::PublishSnapshot()
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

void AMinigameActor::SyncPuppet()
{
	if (PuppetManager)
	{
		PuppetManager->PushState(State);
	}
}

TSharedRef<FJsonObject> AMinigameActor::BuildSnapshot() const
{
	TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("actor"), GetName());
	Obj->SetNumberField(TEXT("state"), static_cast<int32>(State));

	if (GameConnection)
	{
		TArray<TSharedPtr<FJsonValue>> Participants;
		for (const FString& UPID : GameConnection->GetParticipantUPIDs())
		{
			Participants.Add(MakeShared<FJsonValueString>(UPID));
		}
		Obj->SetArrayField(TEXT("participants"), Participants);
	}
	return Obj;
}

// ---- Local read hooks ----

APawnController* AMinigameActor::GetLocalController() const
{
	if (!CachedController)
	{
		CachedController = Cast<APawnController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
	}
	return CachedController;
}

FString AMinigameActor::GetLocalPlayerUPID() const
{
	const APawnController* PC = GetLocalController();
	return PC ? PC->GetPlayerUPID() : FString();
}

bool AMinigameActor::IsLocalPlayerParticipant() const
{
	return GameConnection ? GameConnection->IsParticipant(GetLocalPlayerUPID()) : false;
}

bool AMinigameActor::HasFreeSlot() const
{
	return GameConnection ? GameConnection->GetFreeSlotCount() > 0 : false;
}

bool AMinigameActor::CanLocalPlayerConnect() const
{
	const bool bFinished = (State == EMinigameState::Completed);
	return !bFinished && HasFreeSlot() && !IsLocalPlayerParticipant();
}

bool AMinigameActor::IsPlayerNear()
{
	const APawnController* PC = GetLocalController();
	const ACharVR* Pawn = PC ? PC->GetVRPawn() : nullptr;
	if (!Pawn)
	{
		return false;
	}
	return FVector::Dist(Pawn->GetActorLocation(), GetActorLocation()) < 300.0f;
}

TArray<FString> AMinigameActor::GetActivePlayers()
{
	return ActivePlayers;
}
