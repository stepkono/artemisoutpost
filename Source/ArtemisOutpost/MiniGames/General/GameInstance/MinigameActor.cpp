// Fill out your copyright notice in the Description page of Project Settings.

#include "MinigameActor.h"
#include "ArtemisOutpost/MiniGames/MiniGameComponents/ConnectionComponent/ConnectionComponent.h"
#include "ArtemisOutpost/MiniGames/MiniGameComponents/PuppetManagerComponent/MinigamePuppetManagerComponent.h"
#include "ArtemisOutpost/Moon/MoonBuildings/MoonMiniGamesManager.h"
#include "ArtemisOutpost/Networking/ClientServerConnection/NetUtils.h"
#include "ArtemisOutpost/Player/PawnVR/ACharVR.h"
#include "ArtemisOutpost/Player/PlayerController/PawnController.h"
#include "ArtemisOutpost/Player/PlayerController/PlayerControllerComponents/MiniGameInteraction/MinigamePlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "EnhancedInputSubsystems.h"
#include "EngineUtils.h"
#include "EnhancedPlayerInput.h"

DEFINE_LOG_CATEGORY(LogMinigame);

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
		RegisterWithMiniGamesManager();

		GameConnection->CanJoinPredicate.BindUObject(this, &AMinigameActor::ServerHandleCanJoin);
		GameConnection->OnParticipantJoined.AddUObject(this, &AMinigameActor::ServerHandleParticipantJoined);
		GameConnection->OnParticipantLeft.AddUObject(this, &AMinigameActor::ServerHandleParticipantLeft);
	}
}

FInstancedStruct AMinigameActor::MakeInitialTypeData() const
{
	return FInstancedStruct();
}

void AMinigameActor::RegisterWithMiniGamesManager()
{
	UMoonMiniGamesManager* Manager = GetWorld() ? GetWorld()->GetSubsystem<UMoonMiniGamesManager>() : nullptr;
	if (!Manager)
	{
		UE_LOG(LogTemp, Error, TEXT("MiniGameActor: MoonBuildingsManager subsystem not found; not registered."));
		return;
	}

	AGeoRefsManager* GeoRefsManager = nullptr;
	for (TActorIterator<AGeoRefsManager> It(GetWorld()); It; ++It)
	{
		GeoRefsManager = *It;
	}

	if (!GeoRefsManager)
	{
		// The record would then store a bogus origin position, and since the geodetic position is
		// what travels over the network, every peer would place this building at the moon origin.
		UE_LOG(LogMinigame, Error, TEXT("[Register] %s: no AGeoRefsManager in the level -> BuildGeoLocation falls back to ZeroVector. This record is unusable for range checks and for other peers."),
			*GetName());
	}

	FMiniGameRecord Record;
	Record.MGID          = MGID;
	// GEODETIC on purpose: this is the position that gets sent over the network and stays valid no
	// matter where a peer's georeference origin sits. Consumers convert to UE space themselves.
	Record.BuildGeoLocation = GeoRefsManager != nullptr
							? GeoRefsManager->UECoordsToVRMoonCoords(GetActorLocation())
							: FVector::ZeroVector;
	Record.State         = State;
	Record.Type          = GetBuildingType();
	Record.MiniGameData  = MakeInitialTypeData();
	// BuiltByUPID: the placing player is not tracked here yet; fill in once the build tool passes it.

	UE_LOG(LogMinigame, Log, TEXT("[Register] %s (%s) MGID=%s | BuildGeoLocation=%s | ActorLocation(UEWorld)=%s | HasTypeData=%s"),
		*GetName(),
		*UEnum::GetValueAsString(Record.Type),
		*Record.MGID.ToString(EGuidFormats::DigitsWithHyphens),
		*Record.BuildGeoLocation.ToString(),
		*GetActorLocation().ToString(),
		Record.MiniGameData.IsValid() ? TEXT("yes") : TEXT("NO"));

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
		UE_LOG(LogMinigame, Warning, TEXT("[Input] %s: DROPPED %s from '%s' -> %s"),
			*GetName(), *UEnum::GetValueAsString(Input.Type), *UPID,
			!HasAuthority() ? TEXT("no authority (this is not the server)")
							: *FString::Printf(TEXT("state is %s, not Active"), *UEnum::GetValueAsString(State)));
		return;
	}

	if (!GameConnection || !GameConnection->IsParticipant(UPID))
	{
		UE_LOG(LogMinigame, Warning, TEXT("[Input] %s: DROPPED %s -> UPID '%s' is not a participant."),
			*GetName(), *UEnum::GetValueAsString(Input.Type), *UPID);
		return;
	}

	ApplyInput(UPID, Input);
	PublishSnapshot();
}

void AMinigameActor::ProcessInput(UInputAction* InputAction, EInputActionType TriggerEvent)
{
	// Base does nothing; concrete minigames interpret their own input.
}

void AMinigameActor::SubmitInput(const FMinigameInput& Input)
{
	APawnController* PC = GetLocalController();
	if (!PC)
	{
		return;
	}
	if (UMinigamePlayerController* Controller = PC->GetMinigamePlayerController())
	{
		Controller->ServerSubmitInput(this, Input);
	}
}

FVector2D AMinigameActor::GetLocalActionValue(const UInputAction* Action) const
{
	if (!Action)
	{
		return FVector2D::ZeroVector;
	}

	const APawnController* PC = GetLocalController();
	ULocalPlayer* LP = PC ? PC->GetLocalPlayer() : nullptr;
	if (!LP)
	{
		return FVector2D::ZeroVector;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	UEnhancedPlayerInput* PlayerInput = Subsystem ? Subsystem->GetPlayerInput() : nullptr;
	if (!PlayerInput)
	{
		return FVector2D::ZeroVector;
	}

	return PlayerInput->GetActionValue(Action).Get<FVector2D>();
}

bool AMinigameActor::ServerHandleCanJoin(const FString& UPID, FText& OutReason)
{
	// Additional joiners are always fine (slot rules already checked by the connection). Only the
	// first joiner, which starts the task, must satisfy the game preconditions.
	if (State != EMinigameState::Idle)
	{
		UE_LOG(LogMinigame, Verbose, TEXT("[CanJoin] %s: already running (state=%s) -> joining an active game, no CanStart check."),
			*GetName(), *UEnum::GetValueAsString(State));
		return true;
	}

	const bool bCanStart = CanStart(UPID, OutReason);
	if (!bCanStart)
	{
		UE_LOG(LogMinigame, Warning, TEXT("[CanJoin] %s: REFUSED for UPID '%s' -> CanStart said no: %s"),
			*GetName(), *UPID, *OutReason.ToString());
	}
	return bCanStart;
}

void AMinigameActor::ServerHandleParticipantJoined(const FString& UPID)
{
	UE_LOG(LogMinigame, Log, TEXT("[Join] %s: UPID '%s' JOINED (%d/%d slots). %s"),
		*GetName(), *UPID,
		GameConnection ? GameConnection->GetParticipantCount() : -1,
		GameConnection ? GameConnection->GetMaxSlots() : -1,
		State == EMinigameState::Idle ? TEXT("Starting the game.") : TEXT("Game already running."));

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

		if (UMoonMiniGamesManager* Manager = GetWorld() ? GetWorld()->GetSubsystem<UMoonMiniGamesManager>() : nullptr)
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
		PC->ActivateMiniGameInput(MiniGameType);
		Controller->OpenUI(this);
		bLocalUIOpen = true;
	}
	else if (!bShouldOpen && bLocalUIOpen)
	{
		PC->DeactivateMiniGameInput();
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

EMiniGameType AMinigameActor::GetType()
{
	return MiniGameType;
}
