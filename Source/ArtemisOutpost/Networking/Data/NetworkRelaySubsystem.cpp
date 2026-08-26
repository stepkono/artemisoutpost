// Fill out your copyright notice in the Description page of Project Settings.


#include "NetworkRelaySubsystem.h"

#include "WebsocketManager.h"
#include "EngineUtils.h"
#include "ArtemisOutpost/Moon/MoonResources/MoonResourcesManager.h"
#include "ArtemisOutpost/Moon/MoonResources/ResourceVeinSpline.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	FString ResourceTypeToString(ERessourceType Type)
	{
		switch (Type)
		{
		case ERessourceType::REGOLITH: return TEXT("REGOLITH");
		default:                       return TEXT("NONE");
		}
	}
}

void UNetworkRelaySubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// Server-only: this relay forwards authoritative state to the web. A pure UE client must not.
	if (InWorld.GetNetMode() == NM_Client)
	{
		return;
	}

	ResourcesManager = InWorld.GetSubsystem<UMoonResourcesManager>();
	if (!ResourcesManager)
	{
		UE_LOG(LogTemp, Error, TEXT("[NetworkRelay] No UMoonResourcesManager — resource deltas will not be relayed."));
		return;
	}

	// Subscribe to the provider's domain events. Providers stay wire-agnostic; only we serialize.
	DiscoveredHandle = ResourcesManager->OnVeinDiscovered.AddUObject(this, &UNetworkRelaySubsystem::HandleVeinDiscovered);
	MinedHandle      = ResourcesManager->OnVeinMined.AddUObject(this, &UNetworkRelaySubsystem::HandleVeinMined);

	UE_LOG(LogTemp, Log, TEXT("[NetworkRelay] Subscribed to resource deltas (server)."));
}

void UNetworkRelaySubsystem::Deinitialize()
{
	if (ResourcesManager)
	{
		ResourcesManager->OnVeinDiscovered.Remove(DiscoveredHandle);
		ResourcesManager->OnVeinMined.Remove(MinedHandle);
	}

	Super::Deinitialize();
}

AWebsocketManager* UNetworkRelaySubsystem::ResolveWebsocket()
{
	if (IsValid(Websocket))
	{
		return Websocket;
	}

	if (const UWorld* World = GetWorld())
	{
		for (TActorIterator<AWebsocketManager> It(World); It; ++It)
		{
			Websocket = *It;
			break;
		}
	}
	return Websocket;
}

void UNetworkRelaySubsystem::HandleVeinDiscovered(UResourceVeinSpline* Vein, const TArray<FVector>& GeoPositions)
{
	if (AWebsocketManager* WS = ResolveWebsocket())
	{
		WS->SendMessage(BuildVeinDeltaJson(TEXT("resourceDiscovered"), Vein, GeoPositions));
	}
}

void UNetworkRelaySubsystem::HandleVeinMined(UResourceVeinSpline* Vein, const TArray<FVector>& GeoPositions)
{
	if (AWebsocketManager* WS = ResolveWebsocket())
	{
		WS->SendMessage(BuildVeinDeltaJson(TEXT("resourceMined"), Vein, GeoPositions));
	}
}

FString UNetworkRelaySubsystem::BuildVeinDeltaJson(const FString& EventType, UResourceVeinSpline* Vein, const TArray<FVector>& Geo) const
{
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("type"), EventType);

	if (Vein && Vein->GetOwner())
	{
		// Stable per-session id so the web can group points into the right vein polyline.
		Root->SetStringField(TEXT("veinId"), Vein->GetOwner()->GetName());
		Root->SetStringField(TEXT("resource"), ResourceTypeToString(Vein->GetResourceType()));
	}

	TArray<TSharedPtr<FJsonValue>> Points;
	for (const FVector& P : Geo)
	{
		const TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetNumberField(TEXT("lon"), P.X);
		O->SetNumberField(TEXT("lat"), P.Y);
		O->SetNumberField(TEXT("height"), P.Z);
		Points.Add(MakeShared<FJsonValueObject>(O));
	}
	Root->SetArrayField(TEXT("points"), Points);

	FString Out;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Root, Writer);
	return Out;
}
