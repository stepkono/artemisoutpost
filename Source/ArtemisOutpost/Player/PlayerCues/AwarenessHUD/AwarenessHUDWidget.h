// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ArtemisOutpost/Player/PlayerCues/PlayerCueTypes.h"
#include "AwarenessHUDWidget.generated.h"

class UPanelWidget;
class UWidget;
class UAwarenessHUDRowWidget;

// One HUD row = one player. Plain data so the BP view only has to draw it.
USTRUCT(BlueprintType)
struct ARTEMISOUTPOST_API FAwarenessHUDRow
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Awareness HUD")
	FString UPID;

	// Colour tag source (the Entwurf's "Farbe & Nummer").
	UPROPERTY(BlueprintReadOnly, Category = "Awareness HUD")
	int32 PlayerNumber = -1;

	UPROPERTY(BlueprintReadOnly, Category = "Awareness HUD")
	EPlayerContext Context = EPlayerContext::AR;

	UPROPERTY(BlueprintReadOnly, Category = "Awareness HUD")
	EPlayerActivity Activity = EPlayerActivity::Idle;

	// Ready-to-display activity text, e.g. "Minigame: Signal Tower", "Zeigt auf Spieler 2".
	UPROPERTY(BlueprintReadOnly, Category = "Awareness HUD")
	FText ActivityText;

	// Discord-like green frame while true.
	UPROPERTY(BlueprintReadOnly, Category = "Awareness HUD")
	bool bTalking = false;

	UPROPERTY(BlueprintReadOnly, Category = "Awareness HUD")
	bool bIsLocalPlayer = false;

	// The full state, for views that want more than the summary.
	UPROPERTY(BlueprintReadOnly, Category = "Awareness HUD")
	FPlayerCueState CueState;
};

/**
 * C++ base for WBP_AwarenessHUD. Receives the rebuilt row list from UAwarenessHUDComponent and
 * renders it into three blocks (AR, VR, R) with the local player's own block on top.
 *
 * Zero-graph setup: set RowWidgetClass (your WBP_AwarenessRow) and name these widgets in the BP:
 *
 *   BlocksBox     VerticalBox   holds the three block roots, they are reordered so the own context is first
 *   BlockRootAR   any widget    the whole AR block (header + rows), child of BlocksBox
 *   BlockRootVR   any widget    same for VR
 *   BlockRootR    any widget    same for R
 *   RowsAR        VerticalBox   where the AR rows are created (inside BlockRootAR)
 *   RowsVR        VerticalBox   same for VR
 *   RowsR         VerticalBox   same for R
 *
 * Every property is optional: a block whose Rows panel is missing is simply skipped. Row widgets are
 * pooled per block, extra ones are collapsed, so the 5 Hz refresh does not create widgets.
 * OnRowsUpdated still fires afterwards for anything custom.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class ARTEMISOUTPOST_API UAwarenessHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Driven by UAwarenessHUDComponent.
	void SetRows(const TArray<FAwarenessHUDRow>& InRows, EPlayerContext InLocalContext);
	void NotifyOpened();
	void NotifyClosed();

	UFUNCTION(BlueprintPure, Category = "Awareness HUD")
	const TArray<FAwarenessHUDRow>& GetRows() const { return Rows; }

	UFUNCTION(BlueprintPure, Category = "Awareness HUD")
	EPlayerContext GetLocalContext() const { return LocalContext; }

	// Rows of one block, sorted by player number.
	UFUNCTION(BlueprintPure, Category = "Awareness HUD")
	TArray<FAwarenessHUDRow> GetRowsForContext(EPlayerContext Context) const;

	// Block order for the view: the local context first, then the other two in AR, VR, R order.
	UFUNCTION(BlueprintPure, Category = "Awareness HUD")
	TArray<EPlayerContext> GetBlockOrder() const;

	// The German one-liner for a player's current activity, with the pointing target folded in.
	UFUNCTION(BlueprintPure, Category = "Awareness HUD")
	static FText MakeActivityText(const FPlayerCueState& State);

	UFUNCTION(BlueprintPure, Category = "Awareness HUD")
	static FText MakeTargetText(const FPointingTarget& Target);

protected:
	// ---- View hooks (implement in WBP_AwarenessHUD if the bound widgets are not enough) ----

	// The row list was rebuilt and the bound blocks were filled (about 5 Hz while open).
	UFUNCTION(BlueprintImplementableEvent, Category = "Awareness HUD")
	void OnRowsUpdated(const TArray<FAwarenessHUDRow>& NewRows, EPlayerContext InLocalContext);

	UFUNCTION(BlueprintImplementableEvent, Category = "Awareness HUD")
	void OnOpened();

	UFUNCTION(BlueprintImplementableEvent, Category = "Awareness HUD")
	void OnClosed();

	// ---- Setup ----

	// WBP_AwarenessRow (reparented to UAwarenessHUDRowWidget). Required for the automatic rows.
	UPROPERTY(EditDefaultsOnly, Category = "Awareness HUD|Setup")
	TSubclassOf<UAwarenessHUDRowWidget> RowWidgetClass;

	// Hide a block entirely while it has no players.
	UPROPERTY(EditDefaultsOnly, Category = "Awareness HUD|Setup")
	bool bCollapseEmptyBlocks = false;

	// ---- Optional bound widgets (match the names in the BP designer) ----
	UPROPERTY(BlueprintReadOnly, Category = "Awareness HUD", meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> BlocksBox;

	UPROPERTY(BlueprintReadOnly, Category = "Awareness HUD", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> BlockRootAR;

	UPROPERTY(BlueprintReadOnly, Category = "Awareness HUD", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> BlockRootVR;

	UPROPERTY(BlueprintReadOnly, Category = "Awareness HUD", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> BlockRootR;

	UPROPERTY(BlueprintReadOnly, Category = "Awareness HUD", meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> RowsAR;

	UPROPERTY(BlueprintReadOnly, Category = "Awareness HUD", meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> RowsVR;

	UPROPERTY(BlueprintReadOnly, Category = "Awareness HUD", meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> RowsR;

private:
	// Fill one block's panel from its rows, reusing pooled row widgets.
	void FillBlock(EPlayerContext Context, UPanelWidget* Panel, UWidget* BlockRoot, TArray<TObjectPtr<UAwarenessHUDRowWidget>>& Pool);

	// Move the three block roots inside BlocksBox into GetBlockOrder().
	void ReorderBlocks();

	UPROPERTY(Transient)
	TArray<FAwarenessHUDRow> Rows;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UAwarenessHUDRowWidget>> PoolAR;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UAwarenessHUDRowWidget>> PoolVR;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UAwarenessHUDRowWidget>> PoolR;

	EPlayerContext LocalContext = EPlayerContext::AR;
};
