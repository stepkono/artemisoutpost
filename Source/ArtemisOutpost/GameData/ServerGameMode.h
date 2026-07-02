// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArtemisOutpost/Miscellaneous/DataTypes.h"
#include "ArtemisOutpost/Player/PawnController.h"
#include "GameFramework/GameModeBase.h"
#include "ServerGameMode.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPlayerJoinDelegate, APawnController*, PawnController);

/**
 * 
 */
UCLASS()
class ARTEMISOUTPOST_API AServerGameMode : public AGameModeBase
{
	GENERATED_BODY()
	
public: 
	virtual void BeginPlay() override;
	
protected: 
	virtual void OnPostLogin(AController* NewPlayer) override;
	
private: 
	UFUNCTION()
	void ProcessNewPlayer(APawnController* PlayerController);
	
protected:
	UPROPERTY(BlueprintReadOnly, Category="Players Management")
	TMap<FString, FArtemisPlayer> PlayersInGame; 

	UPROPERTY(BlueprintReadOnly, Category="GeoRefs Manager")
	AGeoRefsManager* GeoRefsManager;
	
	UPROPERTY(BlueprintAssignable, Category="Players Management")
	FPlayerJoinDelegate PlayerJoinDelegate;
	
private: 
	UPROPERTY()
	TArray<APawnController*> CachedPlayers; 
};
