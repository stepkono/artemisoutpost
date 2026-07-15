// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ConstantSettings.generated.h"

/**
 * 
 */
UCLASS()
class ARTEMISOUTPOST_API UConstantSettings : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	static constexpr float ScanningRadius = 5000.0f;
	static constexpr float SmoothingFactor = 100.0f;

	// The Blueprint accessor
	UFUNCTION(BlueprintPure, Category = "Game Settings | Constants")
	static float GetScanningRadius() 
	{ 
		return ScanningRadius; 
	}
};
