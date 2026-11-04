// Fill out your copyright notice in the Description page of Project Settings.

#include "VisualScaleSyncComponent.h"

#include "Engine/World.h"
#include "TransformationsManager.h"

UVisualScaleSyncComponent::UVisualScaleSyncComponent()
{
	// Ticks independently of its owner's actor tick, so a cue that was switched off still catches up on the zoom.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UVisualScaleSyncComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UTransformationsManager* Manager = GetTransformationsManager())
	{
		Manager->OnWTMShouldResize.AddDynamic(this, &UVisualScaleSyncComponent::HandleWTMShouldResize);
		bBoundToResizeEvent = true;
	}
}

void UVisualScaleSyncComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bBoundToResizeEvent && IsValid(TransformationsManager))
	{
		TransformationsManager->OnWTMShouldResize.RemoveDynamic(this, &UVisualScaleSyncComponent::HandleWTMShouldResize);
	}
	bBoundToResizeEvent = false;

	Super::EndPlay(EndPlayReason);
}

void UVisualScaleSyncComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (const UTransformationsManager* Manager = GetTransformationsManager())
	{
		ApplyScale(Manager->GetCommittedVisualScale());
	}
}

void UVisualScaleSyncComponent::HandleWTMShouldResize(float AbsoluteScalingFactor)
{
	ApplyScale(AbsoluteScalingFactor);
}

void UVisualScaleSyncComponent::ApplyScale(float NewScale)
{
	if (bHasAppliedVisualScale && FMath::IsNearlyEqual(NewScale, AppliedVisualScale))
	{
		return;
	}

	const float PreviousScale = bHasAppliedVisualScale ? AppliedVisualScale : 1.0f;
	AppliedVisualScale = NewScale;
	bHasAppliedVisualScale = true;

	OnVisualScaleChanged.Broadcast(NewScale, PreviousScale);
}

UTransformationsManager* UVisualScaleSyncComponent::GetTransformationsManager()
{
	if (!IsValid(TransformationsManager))
	{
		const UWorld* World = GetWorld();
		TransformationsManager = World ? World->GetSubsystem<UTransformationsManager>() : nullptr;
	}
	return TransformationsManager;
}
