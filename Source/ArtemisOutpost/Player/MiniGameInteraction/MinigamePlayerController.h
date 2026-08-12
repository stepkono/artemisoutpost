// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ArtemisOutpost/Minigame/General/MinigameTypes.h"
#include "MinigamePlayerController.generated.h"

class AMinigameActor;
class UMiniGameUI;
class UMinigameLogicComponent;

// The per-player minigame Controller (MVC), attached to APawnController. Two jobs:
//  1) Transport: routes the local player's intents to the server. A minigame actor is
//     server-owned (§9), so a client can't RPC it directly; these RPCs live on this
//     client-owned component and forward to the target's authoritative connection/logic.
//  2) View lifecycle: on the owning client, opens/closes the screen-space View, wires the
//     model's replicated updates INTO the View (plain data) and the View's OnInput OUT to the
//     server. Neither the View nor the model stores a reference to the other.
UCLASS(ClassGroup = (Minigame), meta = (BlueprintSpawnableComponent))
class ARTEMISOUTPOST_API UMinigamePlayerController : public UActorComponent
{
	GENERATED_BODY()

public:
	UMinigamePlayerController();

	// --- Transport (client -> server) ---
	UFUNCTION(Server, Reliable, Category = "Minigame")
	void ServerRequestEnter(AMinigameActor* Target);

	UFUNCTION(Server, Reliable, Category = "Minigame")
	void ServerRequestLeave(AMinigameActor* Target);

	// Unreliable: input is high-frequency and a dropped step is self-correcting.
	UFUNCTION(Server, Unreliable, Category = "Minigame")
	void ServerSubmitInput(AMinigameActor* Target, FMinigameInput Input);

	// --- View lifecycle (called by the minigame model on the owning client) ---
	void OpenUI(AMinigameActor* Target);
	void CloseUI();

private:
	FString GetOwnerUPID() const;

	// View -> server.
	UFUNCTION()
	void HandleUIInput(FMinigameInput Input);

	// Model -> View (plain-data pushes).
	UFUNCTION()
	void HandleModelStateChanged();

	UFUNCTION()
	void HandleModelAxesUpdated(const TArray<FAxisState>& Axes);

	UPROPERTY(Transient)
	UMiniGameUI* ActiveView;

	UPROPERTY(Transient)
	AMinigameActor* ActiveTarget;

	UPROPERTY(Transient)
	UMinigameLogicComponent* ActiveModel;
};
