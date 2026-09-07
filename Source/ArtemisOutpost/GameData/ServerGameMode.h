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
	virtual void BeginPlay() override;
	
public:
	// Called by the BP after it spawns a first-time player's pawns, so the slot remembers them and a
	// later reconnect can re-attach instead of spawning duplicates.
	UFUNCTION(BlueprintCallable, Category="Players Management")
	void RegisterPlayerPawns(APawnController* PlayerController, AMasterRover* InMasterRover, ACharVR* InVRChar);

	UFUNCTION(BlueprintCallable, Category="Players Management")
	TMap<FString, FArtemisPlayer> GetPlayersInGame();
	
protected:
	virtual void OnPostLogin(AController* NewPlayer) override;

	// Read the client's persistent identity from the ?UPID= login option and stash it on the
	// controller, before OnPostLogin runs, so player slots are keyed by a stable cross-session id.
	virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId,
		const FString& Options, const FString& Portal) override;

private:
	UFUNCTION()
	void ProcessNewPlayer(APawnController* PlayerController);
	
protected:
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
