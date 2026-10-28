// Fill out your copyright notice in the Description page of Project Settings.

#include "VRWorldCue.h"

#include "ArtemisOutpost/Anchors/AnchorsManagerSubsystem.h"

AVRWorldCue::AVRWorldCue()
{
	CueContext = ERVMode::VR;
}

FTransform AVRWorldCue::ApplyXRBaseTilt(const FTransform& RawTransform) const
{
	if (RawTransform.ContainsNaN())
	{
		UE_LOG(LogTemp, Error, TEXT("[WorldCues] %s ApplyXRBaseTilt: input contains NaN, returning it unchanged."), *GetName());
		return RawTransform;
	}

	// Same tilt source, count (artemis.AnchorTiltApplications) and tracking-space rotation as the anchors frame.
	FTransform OutTransform = UAnchorsManagerSubsystem::RawAnchorToTrackedSpace(RawTransform);
	// Deliberately not RawTransform's scale, see the declaration.
	OutTransform.SetScale3D(FVector::OneVector);
	return OutTransform;
}
