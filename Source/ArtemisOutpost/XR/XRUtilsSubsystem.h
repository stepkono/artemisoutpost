// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "XRUtilsSubsystem.generated.h"

/**
 *
 */
UCLASS()
class ARTEMISOUTPOST_API UXRUtilsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// Relative: applies only the CURRENT WTM step (XRScaleFactor) around the cached tracking origin.
	// Correct only when called exactly once per step on a position that was right before it.
	// Used by the TransformationsManager to predict the table center after a queued step.
	UFUNCTION(BlueprintCallable)
	FVector GetXRInvariantPositionRelative(const FVector &WorldPosition) const;

	// Absolute: AuthoredScale (the scale at zoom 1) times the committed zoom of the TransformationsManager.
	// Independent of history, so it is also correct for actors spawned after a zoom.
	UFUNCTION(BlueprintCallable)
	FVector GetXRInvariantScaleAbsolute(const FVector &AuthoredScale) const;

	UFUNCTION(BlueprintCallable)
	FTransform GetXRTransform() const;

	UFUNCTION(BlueprintCallable)
	void SetScaleFactor(float ScaleFactor);

	UFUNCTION(BlueprintCallable)
	float GetScaleFactor();

	UFUNCTION(BlueprintCallable)
	void SetXRTransform(FTransform TrackingToWorldTransform);
	
	UFUNCTION()
	void InitXRTRansform();
	
	UFUNCTION(BlueprintCallable, Category = "XR Tracking Space")
	void ResetXRBaseOrientation();

public:
	UPROPERTY(BlueprintReadOnly)
	FTransform XRTransform;
	
private:
	UPROPERTY()
	float XRScaleFactor = 1;
};
