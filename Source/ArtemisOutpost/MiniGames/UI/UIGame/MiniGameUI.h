// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArtemisOutpost/MiniGames/General/MinigameTypes.h"
#include "Blueprint/UserWidget.h"
#include "MiniGameUI.generated.h"

// View -> Controller input signal. The BP view raises it via EmitInput().
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMiniGameUIInput, FMinigameInput, Input);

// Parent of every per-player screen-space minigame UI (BP children: WBP_UISignalTower, ...).
// Deliberately holds NO reference to the model or the actor — that would couple the View to the
// model graph and break the hierarchy. The controller pushes plain data in via PushAxes/PushState
// and subscribes to OnInput; the View only renders and emits intents.
//
// This base exposes only the widget LIFECYCLE to Blueprint (open / close animations). The raw
// replicated data is handed to the C++ hooks instead, so each concrete game's C++ View can digest
// it and expose ONE ready-to-draw state to its Blueprint rather than making every BP re-derive it.
UCLASS(Abstract)
class ARTEMISOUTPOST_API UMiniGameUI : public UUserWidget
{
	GENERATED_BODY()

public:
	// Raised toward the controller. BP calls EmitInput to fire it.
	UPROPERTY(BlueprintAssignable, Category = "Minigame")
	FOnMiniGameUIInput OnInput;

	UFUNCTION(BlueprintCallable, Category = "Minigame")
	void EmitInput(FMinigameInput Input);

	// Plain data set by the controller on open. Match against an axis OwnerUPID.
	UPROPERTY(BlueprintReadOnly, Category = "Minigame")
	FString LocalUPID;

	// Dwell duration, so a View can turn an axis' InToleranceTime into a 0..1 progress ring.
	UPROPERTY(BlueprintReadOnly, Category = "Minigame")
	float DwellSeconds = 0.0f;

	// --- Controller -> View data (C++ only) ---
	// Named Push*, NOT Notify*, to stay distinct from ACoupledAxisMinigameActor::NotifyAxesUpdated
	// which is a different function one layer up.
	void PushAxes(const TArray<FAxisData>& InAxes) { HandleAxesUpdated(InAxes); }
	void PushState(EMinigameState NewState) { HandleStateChanged(NewState); }

	// --- Controller -> View input ---
	// True while this View consumes the thumbstick itself (menu highlight navigation). The controller
	// then withholds the stick from the minigame actor, so ONE stick action drives both a menu screen
	// and the rotation gameplay without a second InputAction. Base: the View wants no input.
	virtual bool WantsNavigationInput() const { return false; }

	// Thumbstick step. Only called while WantsNavigationInput() is true.
	virtual void HandleNavigate(FVector2D Axis) {}

	// Trigger / "enter" pressed while this View is open. Return true if the View consumed the press.
	virtual bool ConfirmHighlighted() { return false; }

	// --- Widget lifecycle (the only BP events on this base) ---
	UFUNCTION(BlueprintImplementableEvent, Category = "Minigame")
	void OnOpened();

	UFUNCTION(BlueprintImplementableEvent, Category = "Minigame")
	void OnClosed();

protected:
	// C++ hooks a concrete game's View overrides. Base does nothing.
	virtual void HandleAxesUpdated(const TArray<FAxisData>& InAxes) {}
	virtual void HandleStateChanged(EMinigameState NewState) {}
};
