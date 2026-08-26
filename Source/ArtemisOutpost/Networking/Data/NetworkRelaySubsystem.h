// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "NetworkRelaySubsystem.generated.h"

class UMoonResourcesManager;
class AWebsocketManager;
class UResourceVeinSpline;

/**
 * Server-only relay: the single place that turns authoritative game-state deltas into outbound
 * web messages. It holds references to the domain data providers (UMoonResourcesManager today,
 * fog/buildings later), subscribes to their events, serializes the payload, and hands it to the
 * WebsocketManager. Direction is one-way: providers broadcast domain events and never know about
 * the wire; only this relay knows the JSON format and the socket.
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

private:
	// Lazily find the WebsocketManager actor (it may connect after this subsystem starts).
	AWebsocketManager* ResolveWebsocket();

	// Provider event handlers -> serialize + send.
	void HandleVeinDiscovered(UResourceVeinSpline* Vein, const TArray<FVector>& GeoPositions);
	void HandleVeinMined(UResourceVeinSpline* Vein, const TArray<FVector>& GeoPositions);

	FString BuildVeinDeltaJson(const FString& EventType, UResourceVeinSpline* Vein, const TArray<FVector>& Geo) const;

	UPROPERTY()
	UMoonResourcesManager* ResourcesManager = nullptr;

	UPROPERTY()
	AWebsocketManager* Websocket = nullptr;

	FDelegateHandle DiscoveredHandle;
	FDelegateHandle MinedHandle;
};
