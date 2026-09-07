// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "NetworkRelaySubsystem.generated.h"

class AWebsocketManager;

/**
 * Server-only relay: the single place that turns authoritative game-state deltas into outbound
 * web messages. Provider events are no longer subscribed here directly, the DataAggregator is the
 * single subscriber for every provider, and dispatches to the wire through this relay's
 * WebsocketManager where needed. Direction is one-way: providers broadcast domain events and
 * never know about the wire; only this relay knows the socket.
 *
 * Runs only where there is authority (listen/dedicated server or standalone) — a pure UE client
 * must not relay. Gated in OnWorldBeginPlay.
 */
UCLASS()
class ARTEMISOUTPOST_API UNetworkRelaySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	// Lazily find the WebsocketManager actor (it may connect after this subsystem starts).
	AWebsocketManager* ResolveWebsocket();

private:
	UPROPERTY()
	AWebsocketManager* Websocket = nullptr;
};
