// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArtemisOutpost/MiniGames/UI/UIGame/CoupledAxisMinigameUI.h"
#include "ArtemisOutpost/MiniGames/Games/Habitat/HabitatTypes.h"
#include "HabitatUI.generated.h"

/**
 * C++ base for WBP_UIHabitat. Selection, highlight, navigation, the rotate page and the own-axis
 * ring come from UCoupledAxisMinigameUI. This class adds the two read-outs that make the Habitat
 * cooperative on screen:
 *  - the partner presence (waiting vs levelling), derived from the replicated ownership with the SAME
 *    rule the server uses (HabitatRules), so the "Warte auf Partner" text can never disagree with
 *    the server's gate;
 *  - the shared "Wasserwaage" bubble, i.e. BOTH axes' signed tilt plus the common dwell progress,
 *    which both players see identically because it is built from replicated values only.
 *
 * Holds no reference to the actor. Everything arrives as plain data through PushAxes / PushState.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class ARTEMISOUTPOST_API UHabitatUI : public UCoupledAxisMinigameUI
{
	GENERATED_BODY()

public:
	UHabitatUI(const FObjectInitializer& ObjectInitializer);

	// Current phase as this client derives it from the pushed data.
	UFUNCTION(BlueprintPure, Category = "Habitat UI")
	EHabitatPhase GetDerivedPhase() const { return LastPhase; }

protected:
	virtual void BroadcastGameSpecific(bool bFirst) override;

	// ---- View hooks. One job each. Implement in WBP_UIHabitat. ----

	// PARTNER. Fires only when the derived phase changed. Leveling = both players are on the rotation
	// screen and the axes move. WaitingForPartner = show the waiting text on the rotate page, the
	// stick does nothing yet. Inactive = the game is not Active (only seen around open/close).
	UFUNCTION(BlueprintImplementableEvent, Category = "Habitat UI")
	void OnPhaseChanged(EHabitatPhase NewPhase);

	// LEVEL BUBBLE. Fires only when a tilt or the shared dwell moved. Tilts are SIGNED degrees
	// (-180..180, 0 = level) from the REPLICATED axis values, so both players see the same bubble.
	// DwellProgress is the 0..1 fraction of the shared hold, i.e. the smaller of the two axes'
	// in-tolerance times, which is exactly the server's completion condition.
	UFUNCTION(BlueprintImplementableEvent, Category = "Habitat UI")
	void OnLevelUpdated(float PitchDeg, float RollDeg, float DwellProgress);

private:
	EHabitatPhase LastPhase = EHabitatPhase::Inactive;

	// False until OnLevelUpdated fired once. The first Refresh can run before the axes arrived (PushState
	// precedes PushAxes on open), so "first" is tracked per event rather than per Refresh here.
	bool bLevelBroadcastDone = false;

	float LastPitchDeg = 0.0f;
	float LastRollDeg = 0.0f;
	float LastSharedDwell = 0.0f;
};
