// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArtemisOutpost/Networking/Data/WebsocketManager.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Types/MoonResourceType/MoonResourceProviderData.h"
#include "DataAggregator.generated.h"

/**
 * 
 */
UCLASS(Blueprintable, Category = "Data Manager Subsystem")
class ARTEMISOUTPOST_API UDataAggregator : public UWorldSubsystem
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable, Category = "Data Manager Subsystem")
	void SetWebSocketManager(AWebsocketManager* WebSocketManager);
	
protected:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void OnWorldEndPlay(UWorld& InWorld) override;
	
private: 
	UPROPERTY()
	AWebsocketManager* WS = nullptr;
	
	void HandleNewGameEvent(UProviderDataBase& ProviderData, const EGameEventType GameEvent);
	
	static FString JSONToString(const TSharedPtr<FJsonObject>& JsonObject);
};
