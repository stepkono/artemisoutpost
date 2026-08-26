// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "IWebSocket.h"
#include "ArtemisOutpost/GameData/ArtemisGameState.h"
#include "ArtemisOutpost/Miscellaneous/DataTypes.h"
#include "GameFramework/Actor.h"
#include "WebsocketManager.generated.h"

UCLASS()
class ARTEMISOUTPOST_API AWebsocketManager : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AWebsocketManager();
	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Server -> web outbound. Sends the JSON string if the socket is connected (no-op otherwise).
	// Used by the network relay to forward authoritative state deltas.
	void SendMessage(const FString& Message) const;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

private: 
	UFUNCTION()
	void InitializeWebsocket(); 
	
	UFUNCTION()
	bool ExtractMapBaseCoordinates(const FString& JsonString, FMapBaseCoordinates& MapBaseCoordinates); 
	
	UFUNCTION()
	void ProcessBufferedMessage();
	 
	UFUNCTION()
	void HandleMessage(const FString& Message); 
	
	UFUNCTION()
	void HandleConnection() const; 
	
	static bool ExtractObjectFieldCoordinates(const TSharedPtr<FJsonObject>& JsonObject, FCoordinates& MapCornerCoords);
	
private: 
	TSharedPtr<IWebSocket> Websocket; 
    
	UPROPERTY()
	FString ServerURL = FString(TEXT("ws://rover.idux.uni-luebeck.de:8081"));
	
	// Rate limiting
	FTimerHandle BroadcastTimerHandle;
	FCriticalSection MessageMutex;

	TOptional<FMapBaseCoordinates> LatestCoordinates;

	UPROPERTY()
	AArtemisGameState* GS;
};
