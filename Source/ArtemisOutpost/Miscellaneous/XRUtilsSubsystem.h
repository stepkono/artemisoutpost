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

	UFUNCTION(BlueprintCallable)
	FVector GetXRInvariantPosition(const FVector &WorldPosition) const;

	UFUNCTION(BlueprintCallable)
	FVector GetXRInvariantScale(const FVector &WorldScale) const;

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

public:
	UPROPERTY(BlueprintReadOnly)
	FTransform XRTransform;

private:
	UPROPERTY()
	float XRScaleFactor = 1;
};
