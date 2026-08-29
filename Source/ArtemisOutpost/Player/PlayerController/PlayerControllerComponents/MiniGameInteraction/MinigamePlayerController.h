// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InputAction.h"
#include "Components/ActorComponent.h"
#include "ArtemisOutpost/MiniGames/General/MinigameTypes.h"
#include "MinigamePlayerController.generated.h"

class AMinigameActor;
class UMiniGameUI;

// The per-player minigame Controller (MVC), attached to APawnController. Two jobs:
//  1) Transport: routes the local player's intents to the server. A minigame actor is server-owned
//     (§9), so a client can't RPC it directly; these RPCs live on this client-owned component and
//     forward to the target actor's authoritative connection / input handler.
//  2) View lifecycle: on the owning client, opens/closes the screen-space View, wires the actor's
//     replicated updates INTO the View (plain data) and the View's OnInput OUT to the server.
//     Neither the View nor the actor stores a reference to the other.

UCLASS(Blueprintable, ClassGroup = (Minigame), meta = (BlueprintSpawnableComponent))
class ARTEMISOUTPOST_API UMinigamePlayerController : public UActorComponent
{
	GENERATED_BODY()

public:
	UMinigamePlayerController();

	// --- Transport (client -> server) ---
	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Minigame")
	void ServerRequestEnter(AMinigameActor* Target);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Minigame")
	void ServerRequestLeave(AMinigameActor* Target);

	// Unreliable: input is high-frequency and a dropped step is self-correcting.
	UFUNCTION(BlueprintCallable, Server, Unreliable, Category = "Minigame")
	void ServerSubmitInput(AMinigameActor* Target, FMinigameInput Input);

	// --- View lifecycle (called by the minigame actor on the owning client) ---
	void OpenUI(AMinigameActor* Target);
	void CloseUI();
	
	UFUNCTION(BlueprintPure, Category = "Minigame")	
	bool IsMiniGameActive(); 
	
	UFUNCTION(BlueprintCallable, Category = "Minigame")
	void SubmitInputAction(UInputAction* InputAction, EInputActionType InputActionType);

	// Trigger / "enter" while a minigame View is open. Routed from BP_VRChar's E_TriggerMode switch
	// (MiniGameMode case). Deliberately takes NO intent: Blueprint does not know which screen the
	// View is on, the View does, so it decides what the press means (axis selection screen -> claim
	// the highlighted axis). Keeps one trigger binding correct as more screens are added.
	UFUNCTION(BlueprintCallable, Category = "Minigame")
	void SubmitEnterAction();

private:
	FString GetPlayerUPID() const;

	// Current value of an InputAction from the LOCAL player's Enhanced Input, as a 2D vector. Used to
	// feed the View's menu navigation, which needs the analog value the BP call site does not carry.
	FVector2D GetLocalActionValue(const UInputAction* Action) const;

	// View -> server.
	UFUNCTION()
	void HandleUIInput(FMinigameInput Input);

	// Actor -> View (plain-data pushes).
	UFUNCTION()
	void HandleModelStateChanged();

	UFUNCTION()
	void HandleModelAxesUpdated(const TArray<FAxisData>& Axes);
	
	UPROPERTY()
	bool bMiniGameActive = false; 	
	
	UPROPERTY(Transient)
	UMiniGameUI* ActiveView;

	// The actor is the model: it owns the state and the axes.
	UPROPERTY(Transient)
	AMinigameActor* ActiveTarget;
};
