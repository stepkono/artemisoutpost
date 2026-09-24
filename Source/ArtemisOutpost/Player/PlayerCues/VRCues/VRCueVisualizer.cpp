// Fill out your copyright notice in the Description page of Project Settings.

#include "VRCueVisualizer.h"

#include "Components/SceneComponent.h"
#include "ArtemisOutpost/Anchors/AnchorsManagerSubsystem.h"

AVRCueVisualizer::AVRCueVisualizer()
{
	// Client-local view of replicated data, nothing about it goes over the network.
	bReplicates = false;
	SetActorEnableCollision(false);
	SetActorHiddenInGame(true);

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void AVRCueVisualizer::SetCueVisible(bool bVisible)
{
	if (bCueVisible == bVisible)
	{
		return;
	}
	bCueVisible = bVisible;

	SetActorHiddenInGame(!bVisible);
	SetActorTickEnabled(bVisible);

	UE_LOG(LogTemp, Log, TEXT("[VRCues] %s visible=%d"), *GetName(), bVisible ? 1 : 0);
	OnCueVisibilityChanged(bVisible);
}

void AVRCueVisualizer::SetXRTrackingRotation(FVector Pivot, FRotator Tilt)
{
	XRTrackingPivot = Pivot;
	XRTrackingTilt = Tilt;
}

FTransform AVRCueVisualizer::ApplyXRBaseTilt(const FTransform& RawTransform) const
{
	if (RawTransform.ContainsNaN())
	{
		UE_LOG(LogTemp, Error, TEXT("[VRCues] %s ApplyXRBaseTilt: input contains NaN, returning it unchanged."), *GetName());
		return RawTransform;
	}

	// Same tilt source, count (artemis.AnchorTiltApplications) and tracking-space rotation as the anchors frame.
	FTransform OutTransform = UAnchorsManagerSubsystem::RawAnchorToTrackedSpace(RawTransform);
	// Deliberately not RawTransform's scale, see the declaration.
	OutTransform.SetScale3D(FVector::OneVector);
	return OutTransform;
}
