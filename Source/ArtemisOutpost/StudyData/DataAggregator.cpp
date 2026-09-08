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

	if (PlayerActionsProvider)
	{
		PlayerActionsProvider->OnPlayerActionEvent.AddUObject(this, &UDataAggregator::HandleNewGameEvent);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[DataAggregator] No UPlayerActionProvider at BeginPlay — HMD events will not be aggregated."));
	}

	UE_LOG(LogTemp, Warning, TEXT("[DataAggregator] Subscribed on server (WS=%s)."), WS ? TEXT("found") : TEXT("NULL"));
}

void UDataAggregator::HandleNewGameEvent(UProviderDataBase& ProviderData, const EGameEventType GameEvent)
{	
	const TSharedPtr<FJsonObject> MiniGameDataObject = ProviderData.BuildJsonFromData(GameEvent);
	const FString DataString = JSONToString(MiniGameDataObject);

	// Log the arrival unconditionally (Warning, so it matches the rest of the system and is not
	// stripped/filtered like Log verbosity), then send — or warn if there is no websocket target.
	UE_LOG(LogTemp, Warning, TEXT("DataAggregator: GameEvent %s received; payload=%s"),
		*UEnum::GetValueAsString(GameEvent), *DataString);

	if (WS)
	{
		WS->SendMessage(DataString);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("DataAggregator: WS is NULL — GameEvent %s not sent over websocket."),
			*UEnum::GetValueAsString(GameEvent));
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

	if (PlayerActionsProvider)
	{
		PlayerActionsProvider->OnPlayerActionEvent.RemoveAll(this);
	}
	
	Super::OnWorldEndPlay(InWorld);
}