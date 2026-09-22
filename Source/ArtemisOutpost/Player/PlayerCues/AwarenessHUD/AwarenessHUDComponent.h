// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ArtemisOutpost/Player/PlayerCues/PlayerCueTypes.h"
#include "AwarenessHUDComponent.generated.h"

class UAwarenessHUDWidget;
class UWidgetComponent;
class UInputAction;
class UEnhancedInputComponent;
class APawnController;
class UPlayerCuesManager;

/**
 * The on-demand HUD overview ("who is where, doing what, who talks"). Lives on BOTH pawns; only the
 * local player's active pawn reacts. HOLD the bound action to look, release to close.
 *
 * Rendering: a UWidgetComponent tagged WidgetAnchorTag, authored under the pawn's camera in the BP
 * (hidden by default, positioned by the designer). The widget is created lazily and pushed into it.
 *
 * Data: while open, the rows are rebuilt from GameState->PlayerArray every RefreshInterval seconds.
 * The PlayerState is the only source, so no other class has to know this HUD exists.
 *
 * Input is bound from the pawn's SetupPlayerInputComponent, gated on the cue manager's
 * IsLocalActivePawn.
 */
UCLASS(ClassGroup = (PlayerCues), meta = (BlueprintSpawnableComponent))
class ARTEMISOUTPOST_API UAwarenessHUDComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAwarenessHUDComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Called from the pawn's SetupPlayerInputComponent (local player only). HoldAction: Started opens,
	// Completed closes.
	void BindInput(UEnhancedInputComponent* EnhancedInputComponent);

	UFUNCTION(BlueprintCallable, Category = "Awareness HUD")
	void Open();

	UFUNCTION(BlueprintCallable, Category = "Awareness HUD")
	void Close();

	UFUNCTION(BlueprintPure, Category = "Awareness HUD")
	bool IsOpen() const { return bIsOpen; }

	// Rebuild the rows now (also runs on the timer while open).
	UFUNCTION(BlueprintCallable, Category = "Awareness HUD")
	void Refresh();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// WBP_AwarenessHUD (must reparent to UAwarenessHUDWidget).
	UPROPERTY(EditDefaultsOnly, Category = "Awareness HUD|Setup")
	TSubclassOf<UAwarenessHUDWidget> WidgetClass;

	// Tag on the WidgetComponent authored under this pawn's camera.
	UPROPERTY(EditDefaultsOnly, Category = "Awareness HUD|Setup")
	FName WidgetAnchorTag = TEXT("Awareness_HUD");

	// Hold to view. Started opens, Completed closes.
	UPROPERTY(EditDefaultsOnly, Category = "Awareness HUD|Input")
	TObjectPtr<UInputAction> HoldAction;

	UPROPERTY(EditDefaultsOnly, Category = "Awareness HUD", meta = (ClampMin = "0.05"))
	float RefreshInterval = 0.2f;

private:
	void HandleHoldStarted();
	void HandleHoldCompleted();

	bool EnsureWidget();
	UWidgetComponent* ResolveAnchor() const;

	APawnController* GetLocalController() const;
	UPlayerCuesManager* GetCuesManager() const;
	bool IsLocalActivePawn() const;
	EPlayerContext GetLocalContext() const;

	UPROPERTY(Transient)
	TObjectPtr<UAwarenessHUDWidget> ActiveWidget;

	UPROPERTY(Transient)
	TObjectPtr<UWidgetComponent> AnchorWidgetComp;

	bool bIsOpen = false;
	float RefreshAccum = 0.0f;
};
