// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArtemisOutpost/Miscellaneous/DataTypes.h"
#include "ArtemisOutpost/Player/PlayerController/PawnController.h"
#include "GameFramework/GameModeBase.h"
#include "ServerGameMode.generated.h"

// First-ever join for a UPID: the BP should SPAWN the player's pawns, then call RegisterPlayerPawns.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPlayerJoinDelegate, APawnController*, PawnController);

// A known UPID reconnecting: the BP should RE-ATTACH the existing pawns (from PlayerData) to the new
// controller — NOT spawn new ones.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPlayerReconnectDelegate, APawnController*, PawnController, FArtemisPlayer, PlayerData);

/**
 * 
 */
UCLASS()
class ARTEMISOUTPOST_API AServerGameMode : public AGameModeBase
{
	GENERATED_BODY()
	
public:
	// Sets AArtemisPlayerState as the PlayerStateClass default (the BP child inherits it unless it
	// overrides the property).
	AServerGameMode();

	virtual void BeginPlay() override;
	
	// Called by the BP after it spawns a first-time player's pawns, so the slot remembers them and a
	// later reconnect can re-attach instead of spawning duplicates.
	UFUNCTION(BlueprintCallable, Category="Players Management")
	void RegisterPlayerPawns(APawnController* PlayerController, AMasterRover* InMasterRover, ACharVR* InVRChar);

	UFUNCTION(BlueprintCallable, Category="Players Management")
	TMap<FString, FArtemisPlayer> GetPlayersInGame();

	// Server: the live slot for a UPID, or null. Do not keep the pointer, the map may reallocate on the next join.
	FArtemisPlayer* FindPlayerSlot(const FString& UPID);

protected:
	virtual void OnPostLogin(AController* NewPlayer) override;

	// Runs right after OnPostLogin (ProcessNewPlayer has updated the slot), before the engine spawns and possesses
	// the default AR pawn. A player whose slot says VR is restored into the parked VR char instead, see
	// TryRestoreIntoVR. Everyone else takes the engine path.
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

	// Timeout drop or graceful leave: the slot stays, only its PlayerState is cleared in the roster.
	virtual void Logout(AController* Exiting) override;

	// Read the client's persistent identity from the ?UPID= login option and stash it on the
	// controller, before OnPostLogin runs, so player slots are keyed by a stable cross-session id.
	virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal) override;
	
private:
	UFUNCTION()
	void ProcessNewPlayer(APawnController* PlayerController);

	// Mirrors one PlayersInGame slot into the replicated AArtemisGameState::PlayersRoster.
	void WritePlayerToRoster(const FArtemisPlayer& Player) const;

	// Reconnect after a drop in VR: spawn the AR pawn unpossessed (the game expects both pawns), possess the VR
	// char where it was parked and set the context to VR. False = not a VR restore, use the engine path.
	bool TryRestoreIntoVR(APawnController* PlayerController);

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Players Management")
	TArray<FLinearColor> PlayerColors;
	
	UPROPERTY(BlueprintReadOnly, Category="Players Management")
	TMap<FString, FArtemisPlayer> PlayersInGame; 

	UPROPERTY(BlueprintReadOnly, Category="GeoRefs Manager")
	AGeoRefsManager* GeoRefsManager;
	
	UPROPERTY(BlueprintAssignable, Category="Players Management")
	FPlayerJoinDelegate NewPlayerJoinDelegate;

	UPROPERTY(BlueprintAssignable, Category="Players Management")
	FPlayerReconnectDelegate PlayerReconnectedDelegate;
	
private: 
	UPROPERTY()
	TArray<APawnController*> CachedPlayers; 
};
