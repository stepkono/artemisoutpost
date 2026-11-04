// Fill out your copyright notice in the Description page of Project Settings.


#include "XRUtilsSubsystem.h"

#include "ArtemisOutpost/Moon/Transformation/TransformationsManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "IXRTrackingSystem.h"

void UXRUtilsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

FVector UXRUtilsSubsystem::GetXRInvariantPositionRelative(const FVector &WorldPosition) const
{
	if (!XRTransform.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[XRUtilsSubsystem]: Unable to calculate calibrated position."));
		return WorldPosition;
	}

	const FVector XRPosition = XRTransform.InverseTransformPosition(WorldPosition);
	// const FVector CalibratedXRPosition = XRPosition * (GEngine->XRSystem->GetWorldToMetersScale() / 100);
	const FVector CalibratedXRPosition = XRPosition * XRScaleFactor;
	const FVector CalibratedWorldPosition = XRTransform.TransformPosition(CalibratedXRPosition);

	return CalibratedWorldPosition;
}

FVector UXRUtilsSubsystem::GetXRInvariantScaleAbsolute(const FVector &AuthoredScale) const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	const UTransformationsManager* TransformationsManager = World ? World->GetSubsystem<UTransformationsManager>() : nullptr;
	if (!TransformationsManager)
	{
		UE_LOG(LogTemp, Error, TEXT("[XRUtilsSubsystem]: Unable to retrieve the TransformationsManager. Returning the authored scale."));
		return AuthoredScale;
	}

	const FVector Result = AuthoredScale * TransformationsManager->GetCommittedVisualScale();
	// TODO: temporary diagnostic, remove once the zoom scaling is verified.
	UE_LOG(LogTemp, Warning, TEXT("[XRUtilsSubsystem] ScaleAbsolute: in=%s committed=%f out=%s liveWTM=%f"),
		*AuthoredScale.ToString(), TransformationsManager->GetCommittedVisualScale(), *Result.ToString(),
		UHeadMountedDisplayFunctionLibrary::GetWorldToMetersScale(GetGameInstance()->GetWorld()));
	return Result;
}

void UXRUtilsSubsystem::SetXRTransform(FTransform TrackingToWorldTransform)
{
	XRTransform = TrackingToWorldTransform;
}

FTransform UXRUtilsSubsystem::GetXRTransform() const
{
	return XRTransform;
}

void UXRUtilsSubsystem::SetScaleFactor(float ScaleFactor)
{
	XRScaleFactor = ScaleFactor;
}

float UXRUtilsSubsystem::GetScaleFactor()
{
	return XRScaleFactor;
}

void UXRUtilsSubsystem::InitXRTRansform()
{
	XRTransform = UHeadMountedDisplayFunctionLibrary::GetTrackingToWorldTransform(GetWorld());
	// IXRTrackingSystem::GetTrackingToWorldTransform()
}

void UXRUtilsSubsystem::ResetXRBaseOrientation()
{
	if (GEngine && GEngine->XRSystem.IsValid())
	{
		GEngine->XRSystem->SetBaseOrientation(FQuat::Identity);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("XRUtilsSubsystem: Unable to retrieve GEngine or GEngine->XRSystem. The XRBaseOrientation won't be set to default."))
	}
}