// Fill out your copyright notice in the Description page of Project Settings.


#include "WebsocketManager.h"

#include "WebSocketsModule.h"


// Sets default values
AWebsocketManager::AWebsocketManager()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void AWebsocketManager::BeginPlay()
{
	Super::BeginPlay();
	
	if (const UWorld* World = GetWorld())
	{
		GS = Cast<AArtemisGameState>(World->GetGameState());
		if (!GS)
		{
			UE_LOG(LogTemp, Error, TEXT("WebsocketManager: Failed to cast game state."));
			return;
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("WebsocketManager: Failed to get world."));
		return; 
	}
	
	InitializeWebsocket();
}

// Called every frame
void AWebsocketManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AWebsocketManager::InitializeWebsocket()
{
	GetWorld()->GetTimerManager().SetTimer(
		BroadcastTimerHandle,
		this,
		&AWebsocketManager::ProcessBufferedMessage,
		1.0f / 30.0f,
		true
	);
	
	if (Websocket) return;
	
	if (!FModuleManager::Get().IsModuleLoaded("WebSockets"))
	{
		FModuleManager::LoadModuleChecked<FWebSocketsModule>("WebSockets");
	}
	
	Websocket = FWebSocketsModule::Get().CreateWebSocket(ServerURL);
	
	if (!Websocket.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create WebSocket!"));
		return;
	}
	
	Websocket->OnConnected().AddUObject(this, &AWebsocketManager::HandleConnection);
	
	Websocket->OnConnectionError().AddLambda([](const FString & Error) -> void {
	   UE_LOG(LogTemp, Warning, TEXT("Websocket connection error"));
	});
    
	Websocket->OnClosed().AddLambda([](int32 StatusCode, const FString& Reason, bool bWasClean) -> void {
	   UE_LOG(LogTemp, Warning, TEXT("[WebsocketManager]: Websocket closed with code: %i. %s"), StatusCode, *Reason);
	});
    
	Websocket->OnMessage().AddUObject(this, &AWebsocketManager::HandleMessage);
    
	Websocket->OnRawMessage().AddLambda([](const void* Data, SIZE_T Size, SIZE_T BytesRemaining) -> void {
	   // This code will run when we receive a raw (binary) message from the server.
	});
    
	Websocket->OnMessageSent().AddLambda([](const FString& MessageString) -> void {
	   // This code is called after we sent a message to the server.
	});
    
	// And we finally c
	Websocket->Connect();
}

void AWebsocketManager::HandleConnection() const 
{
	//const FString HandshakeMessage = TEXT("{\"messageType\":\"identify\",\"clientName\":\"unreal\"}");
	const FString HandshakeMessage = TEXT("{\"type\":\"unreal\"}");
	Websocket->Send(HandshakeMessage);
		
	UE_LOG(LogTemp, Warning, TEXT("Websocket connected"));
}

void AWebsocketManager::HandleMessage(const FString& Message)
{
	TSharedPtr<FJsonObject> JsonObject;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Message), JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[WebsocketManager]: Failed to parse JSON: %s"), *Message);
		return;
	}

	// Route by message content
	if (JsonObject->HasField(TEXT("mapview")))
	{
		FMapBaseCoordinates MapBaseCoordinates;
		if (ExtractMapBaseCoordinates(Message, MapBaseCoordinates))
		{
			FScopeLock Lock(&MessageMutex);
			LatestCoordinates = MapBaseCoordinates;
		}
	}
	else if (JsonObject->HasField(TEXT("controls")))
	{
		const TSharedPtr<FJsonObject>* ControlsPtr;
		if (JsonObject->TryGetObjectField(TEXT("controls"), ControlsPtr))
		{
			FControlCommand Command;
			Command.Accelerator = (*ControlsPtr)->GetNumberField(TEXT("accelerator"));
			Command.Steering = (*ControlsPtr)->HasField(TEXT("steering"))
				? (*ControlsPtr)->GetNumberField(TEXT("steering"))
				: 0.0f;
			UE_LOG(LogTemp, Warning, TEXT("[WebsocketManager]: Control command — Accelerator: %f, Steering: %f"), Command.Accelerator, Command.Steering);
			
			if (GS)
			{
				GS->WriteControlCommand(Command);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[WebsocketManager]: Cannot write control command — GameState is not AArtemisGameState (cast failed in BeginPlay)."));
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[WebsocketManager]: Unknown message type: %s"), *Message);
	}
}

bool AWebsocketManager::ExtractMapBaseCoordinates(const FString& JsonString, FMapBaseCoordinates& MapBaseCoordinates) 
{
	TSharedPtr<FJsonObject> SerializedData; 
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(JsonString), SerializedData))
	{
		UE_LOG(LogTemp, Error, TEXT("[WebsocketManager]: Failed to deserialize JSON string: %s"), *JsonString);
		return false;
	}

	// 1. Get the 'mapview' object first
	const TSharedPtr<FJsonObject>* MapViewPtr;
	if (!SerializedData->TryGetObjectField(TEXT("mapview"), MapViewPtr))
	{
		UE_LOG(LogTemp, Error, TEXT("[WebsocketManager]: Could not find 'mapview' field in JSON."));
		return false;
	}
	
	// Use this shared reference to look inside mapview
	TSharedPtr<FJsonObject> MapView = *MapViewPtr;

	const TSharedPtr<FJsonObject>* MapCornerCoords;
	FCoordinates CenterCoords;  
	FCoordinates UpLeftCoords;
	FCoordinates BottomLeftCoords;
	FCoordinates BottomRightCoords;
	float TerrainElevation;
		
	// 2. Extract fields from MapView instead of SerializedData
	if (MapView->TryGetObjectField(TEXT("center"), MapCornerCoords))
	{
		if (!ExtractObjectFieldCoordinates(MapCornerCoords->ToSharedRef(), CenterCoords)) return false; 
	}
	
	if (MapView->TryGetObjectField(TEXT("upLeft"), MapCornerCoords))
	{
		if (!ExtractObjectFieldCoordinates(MapCornerCoords->ToSharedRef(), UpLeftCoords)) return false; 
	}
	
	if (MapView->TryGetObjectField(TEXT("bottomLeft"), MapCornerCoords))
	{
		if (!ExtractObjectFieldCoordinates(MapCornerCoords->ToSharedRef(), BottomLeftCoords)) return false;
	}
	
	if (MapView->TryGetObjectField(TEXT("bottomRight"), MapCornerCoords))
	{
		if (!ExtractObjectFieldCoordinates(MapCornerCoords->ToSharedRef(), BottomRightCoords)) return false;
	}
	
	if (MapView->TryGetNumberField(TEXT("terrainElevation"), TerrainElevation))
	{
		MapBaseCoordinates.TerrainElevation = TerrainElevation;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[WebsocketManager]: Failed to get terrainElevation from mapview."));
		return false; 
	}
	
	MapBaseCoordinates.UpLeft = UpLeftCoords;
	MapBaseCoordinates.BottomLeft = BottomLeftCoords;
	MapBaseCoordinates.BottomRight = BottomRightCoords;
	MapBaseCoordinates.BottomRight.Height = 0; 
	MapBaseCoordinates.Origin = CenterCoords;
		
	return true; 
}

bool AWebsocketManager::ExtractObjectFieldCoordinates(const TSharedPtr<FJsonObject>& JsonObject, FCoordinates& MapCornerCoords)
{
	if (!JsonObject.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[WebsocketManager]: Coordinates object field is not a valid JSON."));
		return false;  
	}
	
	// Get Lat value
	if (!JsonObject->TryGetNumberField(TEXT("lat"), MapCornerCoords.Latitude))
	{
		UE_LOG(LogTemp, Error, TEXT("[WebsocketManager]: Failed to get the latitude number field from the JSON object."));
		return false; 
	}
	
	// Get Lon value
	if (!JsonObject->TryGetNumberField(TEXT("lon"), MapCornerCoords.Longitude))
	{
		UE_LOG(LogTemp, Error, TEXT("[WebsocketManager]: Failed to get longitude number field from the JSON object."));
		return false; 
	}
	
	MapCornerCoords.Height = 0; 
	return true; 
}

void AWebsocketManager::ProcessBufferedMessage()
{
	TOptional<FMapBaseCoordinates> DataToSend;

	{
		FScopeLock Lock(&MessageMutex);
		if (!LatestCoordinates.IsSet())
			return;

		DataToSend = LatestCoordinates;
		LatestCoordinates.Reset();
	}

	if (GS)
	{
		GS->WriteMapCoordinates(DataToSend.GetValue());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[WebsocketManager]: Cannot write map coordinates — GameState is not AArtemisGameState."));
	}
}