// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ArtemisOutpost/Minigame/MinigameTypes.h"
#include "MinigameClientComponent.generated.h"

class AMinigameActor;

// Client -> server transport for minigames, attached to APawnController. A minigame actor is
// server-owned (§9), so a client cannot RPC it directly; Unreal only routes a client Server RPC
// through an actor/component the client OWNS. This component lives on the client-owned
// controller (which carries the persistent UPID) and forwards to the target actor's
// authoritative connection/logic on the server — keeping the transport out of PawnController.
UCLASS(ClassGroup = (Minigame), meta = (BlueprintSpawnableComponent))
class ARTEMISOUTPOST_API UMinigameClientComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMinigameClientComponent();

	UFUNCTION(Server, Reliable, Category = "Minigame")
	void ServerRequestEnter(AMinigameActor* Target);

	UFUNCTION(Server, Reliable, Category = "Minigame")
	void ServerRequestLeave(AMinigameActor* Target);

	// Unreliable: rotation input is high-frequency and a dropped nudge is self-correcting; a
	// Reliable channel would risk buffer overflow while a button is held.
	UFUNCTION(Server, Unreliable, Category = "Minigame")
	void ServerSubmitInput(AMinigameActor* Target, FMinigameInput Input);

private:
	// UPID of the owning controller (the player this RPC acts for). Empty off a valid controller.
	FString GetOwnerUPID() const;
};
