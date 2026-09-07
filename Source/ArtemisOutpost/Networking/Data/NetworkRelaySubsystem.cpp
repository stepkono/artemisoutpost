// Fill out your copyright notice in the Description page of Project Settings.


#include "NetworkRelaySubsystem.h"
#include "WebsocketManager.h"
#include "EngineUtils.h"
#include "ArtemisOutpost/Networking/ClientServerConnection/NetUtils.h"

void UNetworkRelaySubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// Server-only: this relay forwards authoritative state to the web. A pure UE client must not.
	if (ArtemisNet::IsClientContext(InWorld.GetNetMode()))
	{
		return;
	}
}

void UNetworkRelaySubsystem::Deinitialize()
{
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
