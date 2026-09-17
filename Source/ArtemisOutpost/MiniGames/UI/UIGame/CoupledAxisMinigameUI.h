// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArtemisOutpost/MiniGames/UI/UIGame/MiniGameUI.h"
#include "CoupledAxisMinigameUI.generated.h"

// The pages of a coupled-axis View's WidgetSwitcher. Which one shows is NEVER decided by the button
// press — it is derived from the replicated axis ownership + minigame state, so the UI can never
// disagree with the server.
UENUM(BlueprintType)
enum class EMinigameAxisUIScreen : uint8
{
	AxisSelection UMETA(DisplayName = "Axis Selection"),
	Rotate        UMETA(DisplayName = "Rotate"),

	// The task finished while this player was still inside (the Habitat completes with both players
	// present). The trigger on this page means leave. The tower never shows it, because it completes
	// only once the last participant is gone.
	Completed     UMETA(DisplayName = "Completed")
};

// Everything the View knows about ONE axis, ready to draw. Built from the replicated FAxisData plus
// the local player's identity, so the Blueprint never compares UPIDs or angles itself. The same
// struct feeds both pages: the selection page reads Label / bEnabled / bOwnedByLocal / bSolved /
// DisabledReason, the rotation page reads ValueDeg / SignedDeg / DwellProgress / bAligned.
// Named with the Minigame prefix because plain "FAxisView" style names collide with engine types.
USTRUCT(BlueprintType)
struct FMinigameAxisView
{
	GENERATED_BODY()

	// Position in the model's Axes array. Send this with a ClaimAxis intent.
	UPROPERTY(BlueprintReadOnly, Category = "Minigame UI")
	int32 AxisIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Minigame UI")
	FText Label;

	// --- Rotation page ---

	// REPLICATED angle 0..360, never a local stick prediction, so both players see the same ring.
	// Feed it straight into URadialWidget::SetAngle.
	UPROPERTY(BlueprintReadOnly, Category = "Minigame UI")
	float ValueDeg = 0.0f;

	// The same angle unwound to -180..180 (0 = target for a levelling game). For tilt meshes and the
	// Habitat's level bubble.
	UPROPERTY(BlueprintReadOnly, Category = "Minigame UI")
	float SignedDeg = 0.0f;

	// 0..1 fraction of the in-tolerance hold. 0 while the game's rotation gate is closed.
	UPROPERTY(BlueprintReadOnly, Category = "Minigame UI")
	float DwellProgress = 0.0f;

	// SERVER verdict: within the actor's AxisToleranceDeg. Colour the ring from this, never from
	// the angle. The tolerance is tuned in one place, on the actor BP.
	UPROPERTY(BlueprintReadOnly, Category = "Minigame UI")
	bool bAligned = false;

	// --- Both pages ---

	// Held by the local player. On the rotation page this is "my" ring.
	UPROPERTY(BlueprintReadOnly, Category = "Minigame UI")
	bool bOwnedByLocal = false;

	// Permanently finished. Style as done (tick, green), not as blocked.
	UPROPERTY(BlueprintReadOnly, Category = "Minigame UI")
	bool bSolved = false;

	// --- Selection page ---

	// False = not selectable, draw greyed out. Held by another participant or already solved.
	UPROPERTY(BlueprintReadOnly, Category = "Minigame UI")
	bool bEnabled = true;

	// Why this option is disabled. Show it on the button.
	UPROPERTY(BlueprintReadOnly, Category = "Minigame UI")
	FText DisabledReason;
};

/**
 * C++ View base shared by every coupled-axis game (USignalTowerUI, UHabitatUI). Owns all selection
 * logic — the option list, which options are greyed out, the highlight (incl. auto-preselecting the
 * only viable axis), joystick navigation, the ClaimAxis intent and which page the WidgetSwitcher
 * shows — and hands the Blueprint ready-to-draw data per PAGE.
 *
 * Blueprint events are grouped by the page they feed and each fires ONLY when the data it carries
 * actually changed:
 *   OnScreenChanged                 which page is up
 *   OnSelection*                    the axis-selection page (list, frame, press feedback)
 *   OnRotationAxesUpdated           the rotation page (angles, dwell, aligned, ownership)
 * A pure rotation tick therefore raises OnRotationAxesUpdated and nothing else.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class ARTEMISOUTPOST_API UCoupledAxisMinigameUI : public UMiniGameUI
{
	GENERATED_BODY()

public:
	UCoupledAxisMinigameUI(const FObjectInitializer& ObjectInitializer);

	// --- UMiniGameUI input hooks (called by UMinigamePlayerController) ---

	// The stick belongs to this View only while the selection screen is up. On the rotate screen it
	// falls through to ACoupledAxisMinigameActor::ProcessInput, which turns the owned axis.
	virtual bool WantsNavigationInput() const override;
	virtual void HandleNavigate(FVector2D Axis) override;

	// Selection screen: claim the highlighted axis.
	// Rotate / Completed screen: the trigger IS the accept — the player is done, so the press asks the
	// controller to leave the minigame. Whether the task was actually solved is decided server-side;
	// the View never judges the task.
	virtual EMiniGameUIAction ConfirmHighlighted() override;

	UFUNCTION(BlueprintPure, Category = "Minigame UI")
	EMinigameAxisUIScreen GetCurrentScreen() const { return CurrentScreen; }

	// Latest per-axis view data, same content the events carry. For BP code that runs outside an event.
	UFUNCTION(BlueprintPure, Category = "Minigame UI")
	const TArray<FMinigameAxisView>& GetAxisViews() const { return AxisViews; }

	// The axis the local player holds. Returns false (and a default struct) when none is held.
	UFUNCTION(BlueprintPure, Category = "Minigame UI")
	bool GetOwnAxis(FMinigameAxisView& OutAxis) const;

protected:
	virtual void HandleAxesUpdated(const TArray<FAxisData>& InAxes) override;
	virtual void HandleStateChanged(EMinigameState NewState) override;

	// Extension point for concrete Views: called at the end of every Refresh, after the shared events
	// were raised. Compare against your own cached values and raise only what moved. bFirst is true
	// on the initial broadcast, so the BP gets one full set of events on open.
	virtual void BroadcastGameSpecific(bool bFirst) {}

	// ---- View hooks. Implement in the WBP child. ----

	// PAGE. Fires only when the WidgetSwitcher must show a different page.
	// Rare: on open, on claim, on completion.
	UFUNCTION(BlueprintImplementableEvent, Category = "Minigame UI")
	void OnScreenChanged(EMinigameAxisUIScreen NewScreen);

	// SELECTION PAGE, list content. Fires only when an option's availability, ownership or solved
	// state changed, i.e. when someone claims or releases an axis or an axis gets solved. NOT on
	// rotation. Rebuild the buttons from Label / bEnabled / bOwnedByLocal / bSolved / DisabledReason.
	UFUNCTION(BlueprintImplementableEvent, Category = "Minigame UI|Selection Page")
	void OnSelectionOptionsChanged(const TArray<FMinigameAxisView>& Axes);

	// SELECTION PAGE, frame. Fires only when the highlighted entry moved (stick flick, or
	// auto-preselect when the other player takes an option). OldIndex == -1 means no previous highlight.
	UFUNCTION(BlueprintImplementableEvent, Category = "Minigame UI|Selection Page")
	void OnSelectionHighlightChanged(int32 NewIndex, int32 OldIndex);

	// SELECTION PAGE, press feedback. One-shot. bWasEnabled == false means play a "denied" bump.
	UFUNCTION(BlueprintImplementableEvent, Category = "Minigame UI|Selection Page")
	void OnSelectionConfirmed(int32 Index, bool bWasEnabled);

	// ROTATION PAGE. Fires whenever any axis' ValueDeg, DwellProgress, bAligned or bOwnedByLocal
	// changed, with ALL axes, so a shared read-out (the Habitat's level bubble) and the own ring are
	// drawn from one call. Typical BP: find the entry with bOwnedByLocal (or call GetOwnAxis),
	// SetAngle(ValueDeg) on the radial widget, SetFillColor(green if bAligned).
	UFUNCTION(BlueprintImplementableEvent, Category = "Minigame UI|Rotation Page")
	void OnRotationAxesUpdated(const TArray<FMinigameAxisView>& Axes);

	// ---- Designer-editable content ----

	// Label per axis index. Missing entries fall back to a generic "Achse N".
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minigame UI|Content")
	TArray<FText> AxisLabels;

	// Reason attached to an option held by another participant.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minigame UI|Content")
	FText AxisTakenReason;

	// Reason attached to an option that is already solved.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minigame UI|Content")
	FText AxisSolvedReason;

	// ---- Navigation tuning (mirrors UToolsHUDWidget) ----

	// Stick magnitude that triggers a step.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minigame UI|Navigation", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float NavStepThreshold = 0.5f;

	// Below this magnitude the stick counts as "centred", which re-arms an immediate next step.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minigame UI|Navigation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float NavReleaseThreshold = 0.3f;

	// Seconds between auto-repeat steps while the stick is held past the threshold.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minigame UI|Navigation", meta = (ClampMin = "0.05"))
	float NavRepeatDelay = 0.2f;

	// ---- Read access for concrete Views ----

	const TArray<FAxisData>& GetCachedAxes() const { return CachedAxes; }
	EMinigameState GetCachedState() const { return CachedState; }

private:
	// Recompute everything from CachedAxes + CachedState, then raise only the events whose data moved.
	void Refresh();

	// Rebuild AxisViews and re-place the highlight. Raises nothing.
	void RebuildAxisViews();

	// Derive the active page. Raises nothing.
	void UpdateScreen();

	// True when the selection page would render identically (labels are static, so only the fields it
	// reads are compared).
	static bool SelectionEqual(const TArray<FMinigameAxisView>& A, const TArray<FMinigameAxisView>& B);

	// True when the rotation page would render identically.
	static bool RotationEqual(const TArray<FMinigameAxisView>& A, const TArray<FMinigameAxisView>& B);

	// First ENABLED option at or after StartIndex walking in Dir (wraps). INDEX_NONE if none exists,
	// which is what keeps the highlight off a greyed-out axis.
	int32 FindEnabledIndex(int32 StartIndex, int32 Dir) const;

	// Last data pushed in by the controller.
	UPROPERTY(Transient)
	TArray<FAxisData> CachedAxes;

	EMinigameState CachedState = EMinigameState::Idle;

	UPROPERTY(Transient)
	TArray<FMinigameAxisView> AxisViews;

	int32 HighlightIndex = INDEX_NONE;

	EMinigameAxisUIScreen CurrentScreen = EMinigameAxisUIScreen::AxisSelection;

	// False until the first Refresh, so the view gets one full set of events on open even where the
	// computed value happens to equal the default.
	bool bInitialBroadcastDone = false;

	// Time (seconds) of the last highlight step, for auto-repeat timing. Very negative = ready now.
	float LastNavStepTime = -1000.0f;
};
