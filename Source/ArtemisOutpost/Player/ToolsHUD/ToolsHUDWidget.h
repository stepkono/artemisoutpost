// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ToolsHUDTypes.h"
#include "ToolsHUDWidget.generated.h"

// Fired when the player confirms a tile (trigger) that is ENABLED. The owning component listens and
// decides what the action does (page nav, close, or dispatch a gameplay intent). BlueprintAssignable
// so gameplay BP can also react directly.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnToolActionConfirmed, EHUDAction, Action);

// Gating hook. The widget does NOT know about resources; it asks this delegate whether an action is
// currently allowed and, if not, for a reason string. Bound in C++ by UToolsHUDComponent so the
// resource system stays fully decoupled. If unbound, everything is enabled.
DECLARE_DELEGATE_RetVal_TwoParams(bool, FToolAvailabilityDelegate, EHUDAction /*Action*/, FText& /*OutReason*/);

/**
 * C++ base for WBP_ToolsHUD. Owns ALL menu logic — current page, the ordered tile list per page,
 * the highlight index, joystick-flick navigation and resource gating — and drives the BP view via
 * BlueprintImplementableEvents. The view only draws; it never decides state or order.
 *
 * The HUD is client-local: nothing here replicates. Only the confirmed action (broadcast via
 * OnToolActionConfirmed) may cross to the server, and that happens outside this widget.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class ARTEMISOUTPOST_API UToolsHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ---- Driven by UToolsHUDComponent ----

	// Open the menu on a given page and reset the highlight. Fires OnMenuOpened + OnPageBuilt.
	void OpenToPage(EToolHUDPage Page);

	// Switch page without a full open (e.g. Main -> Building). Rebuilds tiles + resets highlight.
	UFUNCTION(BlueprintCallable, Category = "Tools HUD")
	void GoToPage(EToolHUDPage Page);

	// Right-thumbstick step. Debounced so one physical flick = one highlight step (list is read in
	// order: right/down = next, left/up = previous).
	void HandleNavigate(FVector2D Axis);

	// Trigger. Confirms the highlighted tile if enabled (fires OnToolActionConfirmed); on a disabled
	// tile it plays the reject feedback only.
	void HandleConfirm();

	// Re-evaluate every tile's enabled-state (call when resources change while the menu is open).
	UFUNCTION(BlueprintCallable, Category = "Tools HUD")
	void RefreshGating();

	// Called by the component when the menu closes, so the view can play its leave animation.
	void NotifyClosed();

	// The gating hook. UToolsHUDComponent assigns this right after creating the widget.
	FToolAvailabilityDelegate AvailabilityDelegate;

	// ---- Output ----
	UPROPERTY(BlueprintAssignable, Category = "Tools HUD")
	FOnToolActionConfirmed OnToolActionConfirmed;

	// ---- Read access for the view / debug ----
	UFUNCTION(BlueprintPure, Category = "Tools HUD")
	EToolHUDPage GetCurrentPage() const { return CurrentPage; }

	UFUNCTION(BlueprintPure, Category = "Tools HUD")
	int32 GetHighlightIndex() const { return HighlightIndex; }

	UFUNCTION(BlueprintPure, Category = "Tools HUD")
	const TArray<FToolTile>& GetTiles() const { return CurrentTiles; }

protected:
	// ---- View hooks (implement the visuals in WBP_ToolsHUD) ----

	// The active page changed and its tiles were (re)built. Switch your WidgetSwitcher to Page and
	// (re)populate the tile visuals from Tiles (text, icon, enabled style).
	UFUNCTION(BlueprintImplementableEvent, Category = "Tools HUD")
	void OnPageBuilt(EToolHUDPage Page, const TArray<FToolTile>& Tiles);

	// The highlighted tile moved. Move your selection frame from OldIndex to NewIndex.
	UFUNCTION(BlueprintImplementableEvent, Category = "Tools HUD")
	void OnHighlightChanged(int32 NewIndex, int32 OldIndex);

	// The highlighted tile was confirmed. bWasEnabled = false means play a "denied" bump instead of
	// the normal press animation.
	UFUNCTION(BlueprintImplementableEvent, Category = "Tools HUD")
	void OnTileConfirmed(int32 Index, bool bWasEnabled);

	// Menu became visible / hidden (enter / leave animations).
	UFUNCTION(BlueprintImplementableEvent, Category = "Tools HUD")
	void OnMenuOpened();

	UFUNCTION(BlueprintImplementableEvent, Category = "Tools HUD")
	void OnMenuClosed();

	// ---- Designer-editable content ----

	// Per-action label overrides. Any action left unset falls back to a built-in default label.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tools HUD|Content")
	TMap<EHUDAction, FText> ActionLabels;

	// Per-action tile icon. Assign the artwork here; unset actions render with a null icon.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tools HUD|Content")
	TMap<EHUDAction, TObjectPtr<UTexture2D>> ActionIcons;

	// ---- Navigation tuning ----

	// Stick magnitude that triggers a step.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tools HUD|Navigation", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float NavStepThreshold = 0.5f;

	// Below this magnitude the stick counts as "centred", which re-arms an immediate next step.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tools HUD|Navigation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float NavReleaseThreshold = 0.3f;

	// Seconds between auto-repeat steps while the stick is held past the threshold.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tools HUD|Navigation", meta = (ClampMin = "0.05"))
	float NavRepeatDelay = 0.2f;

	// Wrap highlight from last->first and first->last.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tools HUD|Navigation")
	bool bWrapNavigation = true;

private:
	// The fixed, ordered action set for a page. This is the single source of truth for tile order.
	static TArray<EHUDAction> GetActionsForPage(EToolHUDPage Page);

	// Built-in fallback label for an action (used when ActionLabels has no override).
	static FText DefaultLabelFor(EHUDAction Action);

	// Compose CurrentTiles for CurrentPage (label + icon + gating) and push to the view.
	void RebuildTiles();

	// Move the highlight to a valid index and notify the view.
	void SetHighlight(int32 NewIndex);

	// Ask the gating delegate; enabled by default when unbound.
	bool EvaluateEnabled(EHUDAction Action, FText& OutReason) const;

	EToolHUDPage CurrentPage = EToolHUDPage::Main;

	UPROPERTY(Transient)
	TArray<FToolTile> CurrentTiles;

	int32 HighlightIndex = 0;

	// Time (seconds) of the last highlight step, for auto-repeat timing. Very negative = ready now.
	float LastNavStepTime = -1000.0f;
};
