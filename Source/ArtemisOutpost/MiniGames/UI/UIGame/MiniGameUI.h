// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArtemisOutpost/MiniGames/General/MinigameTypes.h"
#include "Blueprint/UserWidget.h"
#include "MiniGameUI.generated.h"

// View -> Controller input signal. The BP view raises it via EmitInput().
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMiniGameUIInput, FMinigameInput, Input);

// Parent of every per-player screen-space minigame UI (BP children: WBP_SignalTowerUI, ...).
// Deliberately holds NO reference to the model or the actor — that would couple the View to the
// model graph and break the hierarchy. The controller feeds it plain data (via the events
// below) and subscribes to OnInput; the View only renders and emits intents.
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

	// Dwell duration so the BP can draw a 0..1 progress ring from an axis' InToleranceTime.
	UPROPERTY(BlueprintReadOnly, Category = "Minigame")
	float DwellSeconds = 0.0f;

	// --- Controller pushes state in via these (plain data, no model/actor refs) ---
	UFUNCTION(BlueprintImplementableEvent, Category = "Minigame")
	void OnOpened();

	UFUNCTION(BlueprintImplementableEvent, Category = "Minigame")
	void OnClosed();

	UFUNCTION(BlueprintImplementableEvent, Category = "Minigame")
	void OnMinigameStateChanged(EMinigameState NewState);

	UFUNCTION(BlueprintImplementableEvent, Category = "Minigame")
	void OnAxesUpdated(const TArray<FAxisData>& Axes);
};
