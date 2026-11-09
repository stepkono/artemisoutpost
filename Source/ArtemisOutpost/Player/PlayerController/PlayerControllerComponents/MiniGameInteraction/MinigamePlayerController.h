// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InputAction.h"
#include "Components/ActorComponent.h"
#include "ArtemisOutpost/MiniGames/General/MinigameTypes.h"
#include "MinigamePlayerController.generated.h"

class AMinigameActor;
class UMiniGameUI;
class UEnhancedInputComponent;
struct FInputActionValue;

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

	// Carries the target because ActiveTarget lives only on the owning client and never replicates, so
	// the server has to be told which actor to leave. Call RequestLeave() instead of this directly.
	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Minigame")
	void ServerRequestLeave(AMinigameActor* Target);

	// Leave the minigame this player is currently in. Client-side entry point, no parameters: uses
	// ActiveTarget and forwards it to ServerRequestLeave. The View then closes on its own once the
	// replicated slot change runs AMinigameActor::RefreshLocalUI -> CloseUI. Logs and does nothing
	// when no minigame is open.
	UFUNCTION(BlueprintCallable, Category = "Minigame")
	void RequestLeave();

	// Unreliable: input is high-frequency and a dropped step is self-correcting.
	UFUNCTION(BlueprintCallable, Server, Unreliable, Category = "Minigame")
	void ServerSubmitInput(AMinigameActor* Target, FMinigameInput Input);

	// --- View lifecycle (called by the minigame actor on the owning client) ---
	void OpenUI(AMinigameActor* Target);
	void CloseUI();
	
	UFUNCTION(BlueprintPure, Category = "Minigame")
	bool IsMiniGameActive();

	// True while this client holds a minigame (from OpenUI until CloseUI), View or not.
	bool HasActiveMinigame() const { return ActiveTarget != nullptr; }

	// Called from ACharVR::SetupPlayerInputComponent (local player only). Binds the generic minigame
	// actions once. They live in IMC_MiniGame, which only the input-mode owner adds (MiniGame activity),
	// so they fire only inside a minigame. Same pattern as UToolsHUDComponent::BindInput.
	void BindInput(UEnhancedInputComponent* EnhancedInputComponent);

	// TRANSITION ONLY: the old per-game BP path. Removed in the second C++ pass.
	UFUNCTION(BlueprintCallable, Category = "Minigame")
	void SubmitInputAction(UInputAction* InputAction, EInputActionType InputActionType);

	// Trigger / "enter" while a minigame View is open. Routed by UPlayerInputModeComponent::HandleTrigger
	// in the MiniGame activity. Deliberately takes NO intent: only the View knows which screen it is on,
	// so it decides what the press means (axis selection screen -> claim the highlighted axis).
	UFUNCTION(BlueprintCallable, Category = "Minigame")
	void SubmitEnterAction();

protected:
	// IA_MG_Stick (Axis2D, right thumbstick, dead zone modifier). View navigation while the View wants
	// it, otherwise the model's stick gesture (ProcessStick).
	UPROPERTY(EditDefaultsOnly, Category = "Minigame|Input")
	TObjectPtr<UInputAction> StickAction;

	// IA_MG_Cancel (bool, right A). Leaves the minigame.
	UPROPERTY(EditDefaultsOnly, Category = "Minigame|Input")
	TObjectPtr<UInputAction> CancelAction;

private:
	void HandleStickTriggered(const FInputActionValue& Value);
	void HandleStickReleased();
	void HandleCancel();

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
