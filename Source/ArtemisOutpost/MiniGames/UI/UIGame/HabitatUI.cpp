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
	// Same rule as the server (HabitatRules::DerivePhase), on the same replicated data.
	const EHabitatPhase Phase = HabitatRules::DerivePhase(GetCachedState(), GetCachedAxes());
	if (bFirst || Phase != LastPhase)
	{
		LastPhase = Phase;
		OnPhaseChanged(Phase);
	}
}

float UHabitatUI::GetSharedDwellProgress() const
{
	const TArray<FMinigameAxisView>& Axes = GetAxisViews();
	if (Axes.Num() == 0)
	{
		return 0.0f;
	}

	float Shared = 1.0f;
	for (const FMinigameAxisView& Axis : Axes)
	{
		Shared = FMath::Min(Shared, Axis.DwellProgress);
	}
	return Shared;
}

#undef LOCTEXT_NAMESPACE
