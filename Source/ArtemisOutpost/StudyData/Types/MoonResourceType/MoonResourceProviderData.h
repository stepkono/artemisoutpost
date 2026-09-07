// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArtemisOutpost/Moon/MoonResources/ResourceVeinSpline.h"
#include "ArtemisOutpost/StudyData/Types/ProviderDataBase.h"
#include "MoonResourceProviderData.generated.h"

/**
 * 
 */
UCLASS()
class ARTEMISOUTPOST_API UMoonResourceProviderData : public UProviderDataBase
{
	GENERATED_BODY()
	
public:
	UPROPERTY()
	UResourceVeinSpline* ResourceVein = nullptr;

	UPROPERTY()
	FString AuthoredByUPID;

	// Geodetic (Lon, Lat, Height) positions of the samples that flipped, for both discovered and mined.
	UPROPERTY()
	TArray<FVector> GeoPositions;
};
