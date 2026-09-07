// Fill out your copyright notice in the Description page of Project Settings.


#include "DataAggregator.h"

#include "EngineUtils.h"
#include "ArtemisOutpost/GameData/ArtemisGameState.h"
#include "ArtemisOutpost/Moon/MoonBuildings/MoonMiniGamesManager.h"
#include "ArtemisOutpost/Moon/MoonResources/MoonResourcesManager.h"
#include "ArtemisOutpost/Moon/MoonScannedArea/MoonScannedAreaManager.h"
#include "ArtemisOutpost/Networking/ClientServerConnection/NetUtils.h"
#include "Providers/PlayerActionProvider/PlayerActionProvider.h"

void UDataAggregator::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
		
	// Only run on server
	if (ArtemisNet::IsClientContext(InWorld.GetNetMode()))
	{
		return; 
	}
	
	for (TActorIterator<AWebsocketManager> It(&InWorld); It; ++It)
	{
		if (AWebsocketManager* WebSocketManager = *It)
		{
			WS = WebSocketManager;  
		}
	}
	
	AArtemisGameState* GameState = InWorld.GetGameState<AArtemisGameState>();
	
	UMoonMiniGamesManager* MiniGameProvider          = InWorld.GetSubsystem<UMoonMiniGamesManager>();
	UMoonResourcesManager* MoonResourceProvider      = InWorld.GetSubsystem<UMoonResourcesManager>();
	UMoonScannedAreaManager* MoonScannedAreaProvider = GameState ? GameState->GetMoonDataManager() : nullptr;
	UPlayerActionProvider* PlayerActionsProvider     = InWorld.GetSubsystem<UPlayerActionProvider>();
	
	MiniGameProvider->OnMinigameRegistered.AddUObject(this, &UDataAggregator::HandleNewGameEvent);
	
	MoonResourceProvider->OnVeinDiscovered.AddUObject(this, &UDataAggregator::HandleNewGameEvent);
	MoonResourceProvider->OnVeinMined.AddUObject(this, &UDataAggregator::HandleNewGameEvent);
}

void UDataAggregator::HandleNewGameEvent(UProviderDataBase& ProviderData, const EGameEventType GameEvent)
{	
	const TSharedPtr<FJsonObject> MiniGameDataObject = ProviderData.BuildJsonFromData(GameEvent);
	const FString DataString = JSONToString(MiniGameDataObject);
	
	if (WS)
	{
		WS->SendMessage(DataString);
	}
}

FString UDataAggregator::JSONToString(const TSharedPtr<FJsonObject>& JsonObject)
{
	FString Out; 
	
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(JsonObject, Writer);
	
	return Out;
}

void UDataAggregator::OnWorldEndPlay(UWorld& InWorld)
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return; 
	}
	
	UMoonMiniGamesManager* MiniGameProvider          = World->GetSubsystem<UMoonMiniGamesManager>();
	UMoonResourcesManager* MoonResourceProvider      = World->GetSubsystem<UMoonResourcesManager>();
	const AArtemisGameState* GameState = World->GetGameState<AArtemisGameState>();
	UMoonScannedAreaManager* MoonScannedAreaProvider = GameState ? GameState->GetMoonDataManager() : nullptr;
	UPlayerActionProvider* PlayerActionsProvider     = World->GetSubsystem<UPlayerActionProvider>();
	
	MiniGameProvider->OnMinigameRegistered.RemoveAll(this);
	MoonResourceProvider->OnVeinDiscovered.RemoveAll(this);
	MoonResourceProvider->OnVeinMined.RemoveAll(this);
	
	Super::OnWorldEndPlay(InWorld);
}