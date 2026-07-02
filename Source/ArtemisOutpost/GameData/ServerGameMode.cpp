// Fill out your copyright notice in the Description page of Project Settings.


#include "ServerGameMode.h"

#include "ArtemisOutpost/Networking/WebsocketManager.h"
#include "ArtemisOutpost/Player/PawnController.h"
#include "Kismet/GameplayStatics.h"

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
	// Assign the member so OnPostLogin's "world ready?" gate (if (GeoRefsManager)) actually passes;
	// otherwise every post-BeginPlay join is cached and never processed.
	GeoRefsManager = GRM;

	// Check if new players joined before BeginPlay() did fire
	while (!CachedPlayers.IsEmpty())
	{
		APawnController* Player = CachedPlayers[0];
		ProcessNewPlayer(Player);
		CachedPlayers.RemoveAt(0, EAllowShrinking::Yes); 
	}
	
	World->SpawnActor(AWebsocketManager::StaticClass()); 
}

void AServerGameMode::OnPostLogin(AController* NewPlayer)
{
	Super::OnPostLogin(NewPlayer);
	
	// The client on the listen server itself not allowed to connect
	if (NewPlayer->IsLocalPlayerController())
	{
		UE_LOG(LogTemp, Warning, TEXT("ServerGameMode: Local client tried to connect to server, since listen server. Aborting. This message should not normally appear."))
		return; 
	}
	
	APawnController* PawnController = Cast<APawnController>(NewPlayer);
	if (!PawnController)
	{
		UE_LOG(LogTemp, Error, TEXT("ServerGameMode: Failed to cast connected controller as custom pawn controller."));
		return; 
	}
	
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
	// First connect 
	if (!PlayersInGame.Find(PlayerController->GetPlayerUPID()))
	{
		FArtemisPlayer NewPlayer; 
		NewPlayer.PawnController = PlayerController;
		NewPlayer.PlayerNumber = PlayersInGame.Num() + 1; 
		
		PlayersInGame.Add(PlayerController->GetPlayerUPID(), NewPlayer); 
	}
	// Reconnect
	else
	{
		FArtemisPlayer* PlayerInGame = PlayersInGame.Find(PlayerController->GetPlayerUPID());
		// Update the players controller
		PlayerInGame->PawnController = PlayerController;
	}
	
	PlayerJoinDelegate.Broadcast(PlayerController);
}
