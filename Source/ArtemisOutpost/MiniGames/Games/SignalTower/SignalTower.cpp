// Fill out your copyright notice in the Description page of Project Settings.

#include "SignalTower.h"

int32 ASignalTower::GetAxisCount() const
{
	return 2;
}

void ASignalTower::InitAxisTargets(TArray<FAxisState>& InAxes) const
{
	// Server-authoritative random targets, written into the replicated Axes array. Earth is always
	// arbitrary; Habitat is a placeholder until a habitat data model exists (then derive the bearing
	// from the chosen habitat via AGeoRefsManager).
	if (InAxes.IsValidIndex(AxisEarth))
	{
		InAxes[AxisEarth].TargetValue = FMath::FRandRange(0.0f, 360.0f);
	}
	if (InAxes.IsValidIndex(AxisHabitat))
	{
		InAxes[AxisHabitat].TargetValue = FMath::FRandRange(0.0f, 360.0f);
	}
}

bool ASignalTower::CanStart(const FString& UPID, FText& OutReason) const
{
	// TODO: gate on regolith cost, buildability and proximity to a habitat once those systems
	// exist. Placeholder: always allowed.
	return Super::CanStart(UPID, OutReason);
}

void ASignalTower::OnComplete()
{
	Super::OnComplete();

	OnTowerActivated();
}
