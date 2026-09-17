// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArtemisOutpost/MiniGames/UI/UIGame/CoupledAxisMinigameUI.h"
#include "ArtemisOutpost/MiniGames/Games/Habitat/HabitatTypes.h"
#include "HabitatUI.generated.h"

/**
 * C++ base for WBP_UIHabitat. Pages, selection, navigation and the per-axis rotation data
 * (angles, signed tilt, dwell, aligned) all come from UCoupledAxisMinigameUI. The level bubble is
 * drawn from OnRotationAxesUpdated: Axes[0].SignedDeg is pitch, Axes[1].SignedDeg is roll, and
 * GetSharedDwellProgress() is the common hold ring.
 *
 * This class adds the one fact that is Habitat-specific: whether the partner is there. It is
 * derived from the replicated ownership with the SAME rule the server uses (HabitatRules), so the
 * "Warte auf Partner" text can never disagree with the server's rotation gate.
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

	// 0..1 fraction of the SHARED hold: the smaller of the two axes' dwell fractions, which is exactly
	// the server's completion condition (both in tolerance at the same time). 0 while waiting.
	UFUNCTION(BlueprintPure, Category = "Habitat UI")
	float GetSharedDwellProgress() const;

protected:
	virtual void BroadcastGameSpecific(bool bFirst) override;

	// PARTNER. Fires only when the derived phase changed. Leveling = both players are on the rotation
	// page and the axes move. WaitingForPartner = show the waiting text on the rotation page, the stick
	// does nothing yet. Inactive = the game is not Active (only seen around open/close).
	UFUNCTION(BlueprintImplementableEvent, Category = "Habitat UI")
	void OnPhaseChanged(EHabitatPhase NewPhase);

private:
	EHabitatPhase LastPhase = EHabitatPhase::Inactive;
};
