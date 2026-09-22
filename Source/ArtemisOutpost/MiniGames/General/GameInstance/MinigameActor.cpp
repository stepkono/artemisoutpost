// Fill out your copyright notice in the Description page of Project Settings.

#include "MinigameActor.h"
#include "ArtemisOutpost/MiniGames/MiniGameComponents/ConnectionComponent/ConnectionComponent.h"
#include "ArtemisOutpost/MiniGames/MiniGameComponents/PuppetManagerComponent/MinigamePuppetManagerComponent.h"
#include "ArtemisOutpost/Moon/MoonBuildings/MoonMiniGamesManager.h"
#include "ArtemisOutpost/Networking/ClientServerConnection/NetUtils.h"
#include "ArtemisOutpost/Player/PawnVR/ACharVR.h"
#include "ArtemisOutpost/Player/PlayerController/PawnController.h"
#include "ArtemisOutpost/Player/PlayerController/PlayerControllerComponents/MiniGameInteraction/MinigamePlayerController.h"
#include "ArtemisOutpost/GameData/ArtemisPlayerState.h"
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
	
	this->Tags.AddUnique(FName("Blocking"));
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

	// Self-heal for the local View. Opening is normally EVENT driven (a slot change replicates ->
	// RefreshLocalUI). That misses one case: this client is a participant WITHOUT a slot change ever
	// happening on this client, i.e. after a reconnect. Slots are keyed on UPID and survive the drop
	// (§7), so the server still holds the player inside, but the fresh client only saw the initial
	// replication, possibly before its own UPID was set. Nothing would ever re-evaluate, the prompt
	// stays hidden (already a participant) and the player is stuck. This catches it a frame later.
	if (ArtemisNet::IsClientContext(GetNetMode()) && !bLocalUIOpen && GameConnection)
	{
		const APawnController* PC = GetLocalController();
		const FString LocalUPID = GetLocalPlayerUPID();

		// Wait for the VR pawn too: right after a reconnect it is re-attached by replication a few
		// frames later, and opening before that would fall back to the screen-space View, which is
		// invisible in the HMD.
		if (PC && PC->GetVRPawn() && !LocalUPID.IsEmpty() && GameConnection->IsParticipant(LocalUPID))
		{
			UE_LOG(LogMinigame, Warning, TEXT("[View] %s (%s): local '%s' holds a slot but no View is open and no slot change is pending -> reopening (reconnect recovery)."),
				*GetName(), ArtemisNet::RoleName(GetNetMode()), *LocalUPID);
			RefreshLocalUI();
		}
	}

	if (ConnectionUIHolder)
	{
		// Prompt visibility, evaluated every tick but logged only on a flip, with every input to the
		// decision. This is the first hop of the enter chain: no prompt, no click, no request.
		const bool bNear = IsPlayerNear();
		const bool bCanConnect = CanLocalPlayerConnect();
		const bool bShow = bNear && bCanConnect;

		// Log on any change of the INPUTS, not only of the result: a client that walks up to the
		// building but never gets the prompt must still print a line saying why (near=yes, canConnect=no).
		const uint8 PromptInputs = (bNear ? 1 : 0) | (bCanConnect ? 2 : 0);
		if (PromptInputs != LastPromptInputs)
		{
			LastPromptInputs = PromptInputs;

			const APawnController* PC = GetLocalController();
			const ACharVR* Pawn = PC ? PC->GetVRPawn() : nullptr;
			UE_LOG(LogMinigame, Log, TEXT("[Prompt] %s (%s): prompt %s for local '%s' -> near=%s (vrPawn=%s, dist=%.0f), state=%s, freeSlot=%s (%d/%d), participant=%s."),
				*GetName(), ArtemisNet::RoleName(GetNetMode()), bShow ? TEXT("SHOWN") : TEXT("HIDDEN"),
				PC ? *PC->GetPlayerUPID() : TEXT("<no controller>"),
				bNear ? TEXT("yes") : TEXT("no"),
				Pawn ? *Pawn->GetName() : TEXT("NULL"),
				Pawn ? FVector::Dist(Pawn->GetActorLocation(), GetActorLocation()) : -1.0f,
				*UEnum::GetValueAsString(State),
				HasFreeSlot() ? TEXT("yes") : TEXT("no"),
				GameConnection ? GameConnection->GetParticipantCount() : -1,
				GameConnection ? GameConnection->GetMaxSlots() : -1,
				IsLocalPlayerParticipant() ? TEXT("yes") : TEXT("no"));
		}

		ConnectionUIHolder->SetShowConnectionUI(bShow);
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
	// Finished for good. The connection prompt is already hidden by CanLocalPlayerConnect, this is
	// the authoritative backstop against a late or replayed join request.
	if (State == EMinigameState::Completed)
	{
		OutReason = NSLOCTEXT("Minigame", "AlreadyCompleted", "Diese Aufgabe ist bereits abgeschlossen.");
		UE_LOG(LogMinigame, Warning, TEXT("[CanJoin] %s: REFUSED for UPID '%s' -> the minigame is already completed."),
			*GetName(), *UPID);
		return false;
	}

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

	// Awareness: this player is now "in minigame X" for every HUD.
	if (AArtemisPlayerState* PS = AArtemisPlayerState::FindByUPID(GetWorld(), UPID))
	{
		PS->ServerSetMinigame(true, MiniGameType);
	}

	OnParticipantJoined(UPID);
	PublishSnapshot();
}

void AMinigameActor::ServerHandleParticipantLeft(const FString& UPID)
{
	OnParticipantLeft(UPID);

	ActivePlayers.Remove(UPID);

	if (AArtemisPlayerState* PS = AArtemisPlayerState::FindByUPID(GetWorld(), UPID))
	{
		PS->ServerSetMinigame(false, MiniGameType);
	}
		
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

void AMinigameActor::OnStateChangedNative(EMinigameState NewState)
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
	// Subclass first, so a derived sub-state (e.g. the Habitat phase) is already consistent when the
	// delegate below wakes the View and the puppet.
	OnStateChangedNative(State);

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
	// Runs on every peer after a slot or state change. The dedicated server has no player controller
	// and returns at the first check, which is expected and not logged.
	APawnController* PC = Cast<APawnController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
	if (!PC)
	{
		if (ArtemisNet::IsClientContext(GetNetMode()))
		{
			UE_LOG(LogMinigame, Error, TEXT("[View] %s (%s): no local APawnController (player controller 0 is null or not an APawnController) -> the View can never open on this client."),
				*GetName(), ArtemisNet::RoleName(GetNetMode()));
		}
		return;
	}

	UMinigamePlayerController* Controller = PC->GetMinigamePlayerController();
	if (!Controller)
	{
		UE_LOG(LogMinigame, Error, TEXT("[View] %s (%s): %s has no UMinigamePlayerController component -> the View can never open on this client."),
			*GetName(), ArtemisNet::RoleName(GetNetMode()), *PC->GetName());
		return;
	}

	const FString LocalUPID = PC->GetPlayerUPID();
	const TArray<FString> Participants = GameConnection ? GameConnection->GetParticipantUPIDs() : TArray<FString>();
	const bool bShouldOpen = GameConnection && GameConnection->IsParticipant(LocalUPID);

	UE_LOG(LogMinigame, Log, TEXT("[View] %s (%s): refresh -> local UPID='%s', participants=[%s], state=%s, shouldOpen=%s, isOpen=%s."),
		*GetName(), ArtemisNet::RoleName(GetNetMode()), *LocalUPID, *FString::Join(Participants, TEXT(", ")),
		*UEnum::GetValueAsString(State), bShouldOpen ? TEXT("yes") : TEXT("no"), bLocalUIOpen ? TEXT("yes") : TEXT("no"));

	if (!bShouldOpen && !bLocalUIOpen && Participants.Num() > 0)
	{
		// Somebody holds a slot, but not us. Correct when it is the other player. Suspicious when this
		// client just pressed the prompt: then the server granted the slot to a UPID this client does
		// not carry, which is an identity mismatch between the ?UPID= login option and the GameInstance.
		if (LocalUPID.IsEmpty())
		{
			UE_LOG(LogMinigame, Warning, TEXT("[View] %s: local UPID is EMPTY, so no slot can ever be recognised as ours. Fix the identity first (see [UPID])."),
				*GetName());
		}
		else
		{
			UE_LOG(LogMinigame, Verbose, TEXT("[View] %s: slots are held by other UPIDs only. If YOU just pressed the prompt on this client, compare '%s' against the UPID in the server's [Join] line."),
				*GetName(), *LocalUPID);
		}
	}

	if (bShouldOpen && !bLocalUIOpen)
	{
		UE_LOG(LogMinigame, Log, TEXT("[View] %s: local player '%s' holds a slot -> ActivateMiniGameInput(%s) + OpenUI."),
			*GetName(), *LocalUPID, *UEnum::GetValueAsString(MiniGameType));
		PC->ActivateMiniGameInput(MiniGameType);
		Controller->OpenUI(this);
		bLocalUIOpen = true;
	}
	else if (!bShouldOpen && bLocalUIOpen)
	{
		UE_LOG(LogMinigame, Log, TEXT("[View] %s: local player '%s' no longer holds a slot -> DeactivateMiniGameInput + CloseUI."),
			*GetName(), *LocalUPID);
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
