// Fill out your copyright notice in the Description page of Project Settings.


#include "ServerGameMode.h"

#include "ArtemisOutpost/Networking/Data/WebsocketManager.h"
#include "ArtemisOutpost/Player/PlayerController/PawnController.h"
#include "GameFramework/OnlineReplStructs.h"
#include "Kismet/GameplayStatics.h"
#include "ArtemisOutpost/GameData/ArtemisPlayerState.h"
#include "ArtemisOutpost/GameData/ArtemisGameState.h"

AServerGameMode::AServerGameMode()
{
	// Per-player awareness state replicated to everyone (see AArtemisPlayerState). If BP_GameMode has
	// an explicit PlayerStateClass override it wins; make sure it is this class or a child of it.
	PlayerStateClass = AArtemisPlayerState::StaticClass();
}

void AServerGameMode::BeginPlay()
{
	Super::BeginPlay();
	
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("ServerGameMode: World was null."));
		return; 
	}
	
	AGeoRefsManager* GRM = Cast<AGeoRefsManager>(UGameplayStatics::GetActorOfClass(World, AGeoRefsManager::StaticClass()));
	if (!GRM)
	{
		UE_LOG(LogTemp, Error, TEXT("ServerGameMode: Failed to cast or find the GeoRefsManager."));
		return;
	}

	GeoRefsManager = GRM;

	// Check if new players joined before BeginPlay() did fire
	while (!CachedPlayers.IsEmpty())
	{
		APawnController* Player = CachedPlayers[0];
		ProcessNewPlayer(Player);
		CachedPlayers.RemoveAt(0, EAllowShrinking::Yes); 
	}
}

FString AServerGameMode::InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal)
{
	const FString Result = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);

	// The client passes its persistent identity as a login option (?UPID=…) on connect/reconnect.
	// Stash it on the controller now — before OnPostLogin — so ProcessNewPlayer keys the slot by it.
	const FString OptionUPID = UGameplayStatics::ParseOption(Options, TEXT("UPID"));
	if (APawnController* PawnController = Cast<APawnController>(NewPlayerController))
	{
		if (!OptionUPID.IsEmpty())
		{
			PawnController->SetUPID(OptionUPID);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ServerGameMode: New player has no ?UPID= login option — slot reuse/reconnect will not work for them."));
		}
	}

	return Result;
}

void AServerGameMode::OnPostLogin(AController* NewPlayer)
{
	// The client on the listen server itself not allowed to connect
	if (NewPlayer && NewPlayer->IsLocalController() && NewPlayer->PlayerState)
	{
		UE_LOG(LogTemp, Warning, TEXT("ServerGameMode: Local client tried to connect to server, since listen server. Aborting."))
		NewPlayer->PlayerState->SetIsOnlyASpectator(true);
		Super::OnPostLogin(NewPlayer);
		return; 
	}
	
	Super::OnPostLogin(NewPlayer);
	
	APawnController* PawnController = Cast<APawnController>(NewPlayer);
	if (!PawnController)
	{
		UE_LOG(LogTemp, Error, TEXT("ServerGameMode: Failed to cast connected controller as custom pawn controller."));
		return; 
	}
	
	// GeoRef is required for later pawn spawning 
	if (GeoRefsManager)
	{
		ProcessNewPlayer(PawnController);
	}
	else
	{
		// Cache new players joining while the world is unavailable 
		CachedPlayers.AddUnique(PawnController);
	}
} 

void AServerGameMode::ProcessNewPlayer(APawnController* PlayerController)
{
	const FString UPID = PlayerController->GetPlayerUPID();
	if (UPID.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("ServerGameMode: ProcessNewPlayer with empty UPID — the ?UPID= login option was missing. Cannot key player slot."));
		return;
	}
	
	PlayerController->SetGeoRefsManager(GeoRefsManager);

	// Player Reconnect
	if (FArtemisPlayer* Existing = PlayersInGame.Find(UPID))
	{ 
		APawnController* OldController = Existing->PawnController;
		if (OldController && OldController != PlayerController)
		{
			// Per-connection AR pawn is disposable. The new controller gets a fresh one right after this, in
			// HandleStartingNewPlayer (engine default spawn, or TryRestoreIntoVR).
			APawnAR* OldAR = OldController->GetARPawn();
			if (IsValid(OldAR))
			{
				OldAR->Destroy();
			}
			
			OldController->UnPossess();
			OldController->Destroy();
		}
		
		Existing->PawnController = PlayerController;

		// A reconnect gets a FRESH PlayerState; give it the same number so the color tag is stable.
		Existing->PlayerState = PlayerController->GetArtemisPlayerState();
		if (AArtemisPlayerState* PS = Existing->PlayerState)
		{
			PS->ServerSetPlayerNumber(Existing->PlayerNumber);
			PS->ServerSetPawns(Existing->VRChar, nullptr, Existing->MasterRover);
		}
		WritePlayerToRoster(*Existing);

		UE_LOG(LogTemp, Warning, TEXT("ServerGameMode: Reconnect for UPID %s (player #%d) — re-attaching existing pawns."), *UPID, Existing->PlayerNumber);
		
		PlayerReconnectedDelegate.Broadcast(PlayerController, *Existing);
	}
	// Player first connect
	else
	{
		FArtemisPlayer NewPlayer;
		NewPlayer.PawnController = PlayerController;
		NewPlayer.PlayerState    = PlayerController->GetArtemisPlayerState();
		NewPlayer.UPID           = UPID;
		NewPlayer.PlayerNumber   = PlayersInGame.Num() + 1;
		NewPlayer.PlayerColor    = PlayerColors.IsValidIndex(PlayersInGame.Num()) ? PlayerColors[PlayersInGame.Num()] : FLinearColor(1, 1, 1);
		PlayersInGame.Add(UPID, NewPlayer);

		if (AArtemisPlayerState* PS = NewPlayer.PlayerState)
		{
			PS->ServerSetPlayerNumber(NewPlayer.PlayerNumber);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("ServerGameMode: %s has no AArtemisPlayerState. Set PlayerStateClass on BP_GameMode to AArtemisPlayerState, awareness cues will not work otherwise."), *PlayerController->GetName());
		}
		WritePlayerToRoster(NewPlayer);
		
		UE_LOG(LogTemp, Warning, TEXT("ServerGameMode: First join for UPID %s (player #%d)."), *UPID, NewPlayer.PlayerNumber);
		
		NewPlayerJoinDelegate.Broadcast(PlayerController);
	}
}

void AServerGameMode::RegisterPlayerPawns(APawnController* PlayerController, AMasterRover* InMasterRover, ACharVR* InVRChar)
{
	if (!PlayerController)
	{
		UE_LOG(LogTemp, Error, TEXT("ServerGameMode: RegisterPlayerPawns called with null controller."));
		return;
	}

	FArtemisPlayer* Player = PlayersInGame.Find(PlayerController->GetPlayerUPID());
	if (!Player)
	{
		UE_LOG(LogTemp, Error, TEXT("ServerGameMode: RegisterPlayerPawns — no slot for UPID %s."), *PlayerController->GetPlayerUPID());
		return;
	}

	Player->MasterRover = InMasterRover;
	Player->VRChar = InVRChar;
	WritePlayerToRoster(*Player);
	
	UE_LOG(LogTemp, Warning, TEXT("ServerGameMode: Registered pawns for UPID %s — Master=%s VRChar=%s."),
		*PlayerController->GetPlayerUPID(), *GetNameSafe(InMasterRover), *GetNameSafe(InVRChar));
}

TMap<FString, FArtemisPlayer> AServerGameMode::GetPlayersInGame()
{
	return PlayersInGame;
}

FArtemisPlayer* AServerGameMode::FindPlayerSlot(const FString& UPID)
{
	return UPID.IsEmpty() ? nullptr : PlayersInGame.Find(UPID);
}

void AServerGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	if (TryRestoreIntoVR(Cast<APawnController>(NewPlayer)))
	{
		return;
	}
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
}

bool AServerGameMode::TryRestoreIntoVR(APawnController* PlayerController)
{
	// The spectator host never restores, and only a slot that last reported VR does.
	if (!PlayerController || MustSpectate(PlayerController))
	{
		return false;
	}
	const FArtemisPlayer* Slot = FindPlayerSlot(PlayerController->GetPlayerUPID());
	if (!Slot || Slot->LastARVRMode != ERVMode::VR)
	{
		return false;
	}

	ACharVR* VRChar = Slot->VRChar;
	if (!IsValid(VRChar))
	{
		UE_LOG(LogTemp, Warning, TEXT("ServerGameMode: UPID %s dropped in VR but its VR char is gone. Restoring into AR instead."), *Slot->UPID);
		return false;
	}

	// The game expects both pawns to exist, so spawn the AR pawn the way RestartPlayer would, but leave it
	// unpossessed. It still gets this player as owner: the owner replicates with the pawn, so the client can tell its
	// own AR pawn apart already in BeginPlay (APawnAR::IsOwnedByLocalPlayer), and the Server RPCs in its BeginPlay are
	// not dropped. A possessed AR pawn has the same owner.
	AActor* StartSpot = FindPlayerStart(PlayerController);
	APawn* DefaultPawn = StartSpot ? SpawnDefaultPawnFor(PlayerController, StartSpot) : nullptr;
	APawnAR* NewARPawn = Cast<APawnAR>(DefaultPawn);
	if (!NewARPawn)
	{
		UE_LOG(LogTemp, Error, TEXT("ServerGameMode: VR restore for UPID %s could not spawn the AR pawn (start spot=%s, spawned=%s). Restoring into AR instead."),
			*Slot->UPID, *GetNameSafe(StartSpot), *GetNameSafe(DefaultPawn));
		if (IsValid(DefaultPawn))
		{
			DefaultPawn->Destroy();
		}
		return false;
	}
	NewARPawn->SetOwner(PlayerController);
	PlayerController->SetARPawn(NewARPawn);

	// The VR char was unpossessed in APawnController::OnNetCleanup and stayed where the player left it. Possess it
	// without moving it. The client detects this as its first possession (APawnController::AcknowledgePossession).
	PlayerController->Possess(VRChar);

	if (AArtemisPlayerState* PS = PlayerController->GetArtemisPlayerState())
	{
		PS->ServerSetXRMode(ERVMode::VR);
	}

	UE_LOG(LogTemp, Warning, TEXT("ServerGameMode: UPID %s (player #%d) dropped in VR, restored into %s at %s. AR pawn %s spawned unpossessed."),
		*Slot->UPID, Slot->PlayerNumber, *VRChar->GetName(), *VRChar->GetActorLocation().ToString(), *NewARPawn->GetName());
	return true;
}

void AServerGameMode::Logout(AController* Exiting)
{
	// Match by PlayerState, not UPID: on a reconnect the old controller can log out after the slot already
	// points at the new PlayerState, and that entry must not be cleared.
	const APlayerState* ExitingPS = Exiting ? Exiting->PlayerState.Get() : nullptr;
	if (ExitingPS)
	{
		for (TPair<FString, FArtemisPlayer>& Pair : PlayersInGame)
		{
			FArtemisPlayer& Player = Pair.Value;
			if (Player.PlayerState != ExitingPS)
			{
				continue;
			}

			Player.PlayerState = nullptr;
			if (Player.PawnController == Exiting)
			{
				Player.PawnController = nullptr;
			}
			WritePlayerToRoster(Player);
			UE_LOG(LogTemp, Warning, TEXT("ServerGameMode: UPID %s (player #%d) disconnected — slot kept, PlayerState cleared."), *Player.UPID, Player.PlayerNumber);
			break;
		}
	}

	Super::Logout(Exiting);
}

void AServerGameMode::WritePlayerToRoster(const FArtemisPlayer& Player) const
{
	if (AArtemisGameState* GS = GetGameState<AArtemisGameState>())
	{
		GS->WritePlayer(Player);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ServerGameMode: GameState is not AArtemisGameState — PlayersRoster not updated for UPID %s."), *Player.UPID);
	}
}