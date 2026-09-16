// Fill out your copyright notice in the Description page of Project Settings.

#include "HabitatUI.h"

#define LOCTEXT_NAMESPACE "HabitatUI"

UHabitatUI::UHabitatUI(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// [0] = Pitch, [1] = Roll (AHabitat::AxisPitch / AxisRoll). The WBP may override these.
	AxisLabels.Add(LOCTEXT("AxisPitch", "Neigung (Pitch)"));
	AxisLabels.Add(LOCTEXT("AxisRoll",  "Rollen (Roll)"));
}

void UHabitatUI::BroadcastGameSpecific(bool bFirst)
{
	const TArray<FAxisData>& Axes = GetCachedAxes();

	// --- Partner presence: same rule as the server (HabitatRules::DerivePhase). ---
	const EHabitatPhase Phase = HabitatRules::DerivePhase(GetCachedState(), Axes);
	if (bFirst || Phase != LastPhase)
	{
		LastPhase = Phase;
		OnPhaseChanged(Phase);
	}

	// --- Shared level bubble: both tilts + the common dwell. ---
	if (Axes.Num() < 2)
	{
		// Axes may legitimately arrive a frame after open. Nothing to draw yet.
		return;
	}

	const float PitchDeg = FMath::UnwindDegrees(Axes[0].Value);
	const float RollDeg  = FMath::UnwindDegrees(Axes[1].Value);

	// The server completes on min() over the axes' in-tolerance times, so the bubble's ring shows
	// that same minimum. While waiting for the partner the server holds the timers at zero, which
	// makes the ring read empty without any extra rule here.
	const float SharedDwell = FMath::Min(GetDwellProgress(0), GetDwellProgress(1));

	if (bLevelBroadcastDone
		&& PitchDeg == LastPitchDeg
		&& RollDeg == LastRollDeg
		&& SharedDwell == LastSharedDwell)
	{
		return;
	}

	bLevelBroadcastDone = true;
	LastPitchDeg = PitchDeg;
	LastRollDeg = RollDeg;
	LastSharedDwell = SharedDwell;

	OnLevelUpdated(PitchDeg, RollDeg, SharedDwell);
}

#undef LOCTEXT_NAMESPACE
