// Fill out your copyright notice in the Description page of Project Settings.


#include "ServerGameMode.h"

#include "ArtemisOutpost/Networking/Data/WebsocketManager.h"
#include "ArtemisOutpost/Player/PlayerController/PawnController.h"
#include "GameFramework/OnlineReplStructs.h"
#include "Kismet/GameplayStatics.h"
#include "ArtemisOutpost/GameData/ArtemisPlayerState.h"

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
		UE_LOG(LogTemp, Warning, TEXT("ServerGameMode: Local client tried to connect to server, since listen server. Aborting. This message should not normally appear."))
		NewPlayer->PlayerState->SetIsOnlyASpectator(true);
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

	if (FArtemisPlayer* Existing = PlayersInGame.Find(UPID))
	{ 
		APawnController* OldController = Existing->PawnController;
		if (OldController && OldController != PlayerController)
		{
			// Per-connection AR pawn is disposable, the new controller already got a fresh default pawn.
			APawnAR* OldAR = OldController->GetARPawn();
			if (IsValid(OldAR))
			{
				OldAR->Destroy();
			}
			
			OldController->UnPossess();
			OldController->Destroy();
		}
		
		Existing->PawnController = PlayerController;

		// A reconnect gets a FRESH PlayerState; give it the same number so the colour tag is stable.
		if (AArtemisPlayerState* PS = PlayerController->GetArtemisPlayerState())
		{
			PS->ServerSetPlayerNumber(Existing->PlayerNumber);
			PS->ServerSetPawns(Existing->VRChar, nullptr, Existing->MasterRover);
		}

		UE_LOG(LogTemp, Warning, TEXT("ServerGameMode: Reconnect for UPID %s (player #%d) — re-attaching existing pawns."), *UPID, Existing->PlayerNumber);
		
		PlayerReconnectedDelegate.Broadcast(PlayerController, *Existing);
	}
	else
	{
		// First connect: create the slot, let the BP spawn the pawns (it then calls RegisterPlayerPawns).
		FArtemisPlayer NewPlayer;
		NewPlayer.PawnController = PlayerController;
		NewPlayer.PlayerNumber = PlayersInGame.Num() + 1;
		PlayersInGame.Add(UPID, NewPlayer);

		if (AArtemisPlayerState* PS = PlayerController->GetArtemisPlayerState())
		{
			PS->ServerSetPlayerNumber(NewPlayer.PlayerNumber);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("ServerGameMode: %s has no AArtemisPlayerState. Set PlayerStateClass on BP_GameMode to AArtemisPlayerState, awareness cues will not work otherwise."), *PlayerController->GetName());
		}
		
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
	UE_LOG(LogTemp, Warning, TEXT("ServerGameMode: Registered pawns for UPID %s — Master=%s VRChar=%s."),
		*PlayerController->GetPlayerUPID(), *GetNameSafe(InMasterRover), *GetNameSafe(InVRChar));
}

TMap<FString, FArtemisPlayer> AServerGameMode::GetPlayersInGame()
{
	return PlayersInGame;
}