// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArtemisOutpost/MiniGames/UI/UIGame/MiniGameUI.h"
#include "SignalTowerUI.generated.h"

// The pages of WBP_UISignalTower's WidgetSwitcher. Which one shows is NEVER decided by the button
// press — it is derived from the replicated axis ownership + minigame state, so the UI can never
// disagree with the server.
UENUM(BlueprintType)
enum class ESignalTowerUIScreen : uint8
{
	AxisSelection UMETA(DisplayName = "Axis Selection"),
	Rotate        UMETA(DisplayName = "Rotate"),
	Completed     UMETA(DisplayName = "Completed")
};

// One selectable axis on the selection screen.
// Named with the Minigame prefix because plain "FAxisOption" collides with the engine's animation
// type (Engine/Public/Animation/AnimTypes.h) — same reason EMiniGameType is not "EBuildingType".
USTRUCT(BlueprintType)
struct FMinigameAxisOption
{
	GENERATED_BODY()

	// Index to send with a ClaimAxis intent. Equals the position in the model's Axes array.
	UPROPERTY(BlueprintReadOnly, Category = "Signal Tower UI")
	int32 AxisIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Signal Tower UI")
	FText Label;

	// False = held by ANOTHER participant. Draw greyed out. Cannot be highlighted or confirmed.
	UPROPERTY(BlueprintReadOnly, Category = "Signal Tower UI")
	bool bEnabled = true;

	// Already owned by the local player.
	UPROPERTY(BlueprintReadOnly, Category = "Signal Tower UI")
	bool bOwnedByLocal = false;

	UPROPERTY(BlueprintReadOnly, Category = "Signal Tower UI")
	FText DisabledReason;
};

/**
 * C++ base for WBP_UISignalTower. Owns all selection logic — the option list, which options are
 * greyed out, the highlight (incl. auto-preselecting the only viable axis), joystick navigation,
 * the ClaimAxis intent and which page the WidgetSwitcher shows.
 *
 * Each BP event below has ONE job and fires ONLY when its own data actually changed. A pure
 * rotation tick therefore raises OnRotationUpdated and nothing else — the option list and the page
 * switch stay silent. That change check is what keeps separate events cheap instead of noisy.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class ARTEMISOUTPOST_API USignalTowerUI : public UMiniGameUI
{
	GENERATED_BODY()

public:
	USignalTowerUI(const FObjectInitializer& ObjectInitializer);

	// --- UMiniGameUI input hooks (called by UMinigamePlayerController) ---

	// The stick belongs to this View only while the selection screen is up. On the rotate screen it
	// falls through to ASignalTower::ProcessInput, which turns the owned axis.
	virtual bool WantsNavigationInput() const override;
	virtual void HandleNavigate(FVector2D Axis) override;
	virtual bool ConfirmHighlighted() override;

protected:
	virtual void HandleAxesUpdated(const TArray<FAxisData>& InAxes) override;
	virtual void HandleStateChanged(EMinigameState NewState) override;

	// ---- View hooks. One job each. Implement in WBP_UISignalTower. ----

	// PAGE. Fires only when the WidgetSwitcher must show a different page.
	// Rare: on open, on claim, on completion.
	UFUNCTION(BlueprintImplementableEvent, Category = "Signal Tower UI")
	void OnScreenChanged(ESignalTowerUIScreen NewScreen);

	// SELECTION LIST. Fires only when an option's label, availability or ownership changed, i.e.
	// when someone claims or releases an axis. NOT on rotation.
	UFUNCTION(BlueprintImplementableEvent, Category = "Signal Tower UI")
	void OnAxisOptionsChanged(const TArray<FMinigameAxisOption>& Options);

	// SELECTION FRAME. Fires only when the highlighted entry moved (stick flick, or auto-preselect
	// when the other player takes an option). OldIndex == -1 means there was no previous highlight.
	UFUNCTION(BlueprintImplementableEvent, Category = "Signal Tower UI")
	void OnHighlightChanged(int32 NewIndex, int32 OldIndex);

	// ROTATION. Fires only while this player owns an axis and its angle or dwell changed.
	// ValueDeg is the REPLICATED angle 0..360, never a local stick prediction, so both players see
	// the same ring. Feed it straight into URadialWidget::SetAngle.
	// DwellProgress is the 0..1 in-tolerance hold fraction.
	UFUNCTION(BlueprintImplementableEvent, Category = "Signal Tower UI")
	void OnRotationUpdated(int32 AxisIndex, float ValueDeg, float DwellProgress);

	// PRESS FEEDBACK. One-shot. bWasEnabled == false means play a "denied" bump instead of a press.
	UFUNCTION(BlueprintImplementableEvent, Category = "Signal Tower UI")
	void OnAxisOptionConfirmed(int32 Index, bool bWasEnabled);

	// ---- Designer-editable content ----

	// Label per axis index. [0] = Earth, [1] = Habitat (ASignalTower::AxisEarth / AxisHabitat).
	// Missing entries fall back to a generic "Achse N".
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Signal Tower UI|Content")
	TArray<FText> AxisLabels;

	// Reason attached to an option held by another participant.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Signal Tower UI|Content")
	FText AxisTakenReason;

	// ---- Navigation tuning (mirrors UToolsHUDWidget) ----

	// Stick magnitude that triggers a step.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Signal Tower UI|Navigation", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float NavStepThreshold = 0.5f;

	// Below this magnitude the stick counts as "centred", which re-arms an immediate next step.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Signal Tower UI|Navigation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float NavReleaseThreshold = 0.3f;

	// Seconds between auto-repeat steps while the stick is held past the threshold.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Signal Tower UI|Navigation", meta = (ClampMin = "0.05"))
	float NavRepeatDelay = 0.2f;

private:
	// Recompute everything from CachedAxes + CachedState, then raise only the events whose data moved.
	void Refresh();

	// Rebuild the option list and re-place the highlight. Raises nothing.
	void RebuildOptions();

	// Derive the active page. Raises nothing.
	void UpdateScreen();

	// Raise OnRotationUpdated if the owned axis' angle or dwell moved. Silent when nothing is owned.
	void BroadcastRotation();

	// True when the two lists would render identically (labels are static, so only the mutable
	// fields are compared).
	static bool OptionsEqual(const TArray<FMinigameAxisOption>& A, const TArray<FMinigameAxisOption>& B);

	// First ENABLED option at or after StartIndex walking in Dir (wraps). INDEX_NONE if none exists,
	// which is what keeps the highlight off a greyed-out axis.
	int32 FindEnabledIndex(int32 StartIndex, int32 Dir) const;

	// Last data pushed in by the controller.
	UPROPERTY(Transient)
	TArray<FAxisData> CachedAxes;

	EMinigameState CachedState = EMinigameState::Idle;

	UPROPERTY(Transient)
	TArray<FMinigameAxisOption> AxisOptions;

	int32 HighlightIndex = INDEX_NONE;

	ESignalTowerUIScreen CurrentScreen = ESignalTowerUIScreen::AxisSelection;

	// False until the first Refresh, so the view gets one full set of events on open even where the
	// computed value happens to equal the default.
	bool bInitialBroadcastDone = false;

	// Last values handed to OnRotationUpdated, so a repeated identical update stays silent.
	int32 LastRotationAxis = INDEX_NONE;
	float LastRotationValue = 0.0f;
	float LastRotationDwell = 0.0f;

	// Time (seconds) of the last highlight step, for auto-repeat timing. Very negative = ready now.
	float LastNavStepTime = -1000.0f;
};
