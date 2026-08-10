// Fill out your copyright notice in the Description page of Project Settings.

#include "ArtemisOutpost/Minigame/SignalTower/SignalTowerLogicComponent.h"

int32 USignalTowerLogicComponent::GetAxisCount() const
{
	return 2;
}

void USignalTowerLogicComponent::InitAxisTargets()
{
	// Server-authoritative random targets, replicated via the Axes array. Earth is always
	// arbitrary; Habitat is a placeholder until a habitat data model exists (then derive the
	// bearing from the chosen habitat via AGeoRefsManager).
	if (Axes.IsValidIndex(AxisEarth))
	{
		Axes[AxisEarth].TargetValue = FMath::FRandRange(0.0f, 360.0f);
	}
	if (Axes.IsValidIndex(AxisHabitat))
	{
		Axes[AxisHabitat].TargetValue = FMath::FRandRange(0.0f, 360.0f);
	}
}

bool USignalTowerLogicComponent::CanStart(const FString& UPID, FText& OutReason) const
{
	// TODO: gate on regolith cost, buildability and proximity to a habitat once those systems
	// exist. Placeholder: always allowed.
	return Super::CanStart(UPID, OutReason);
}

void USignalTowerLogicComponent::OnComplete()
{
	Super::OnComplete();
	
	OnTowerActivated();
}
