// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CesiumGeoreference.h"
#include "ArtemisOutpost/Miscellaneous/DataTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GeoUtils.generated.h"

/**
 * 
 */
UCLASS()
class ARTEMISOUTPOST_API UGeoUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UGeoUtils();
	~UGeoUtils();

	UFUNCTION(BlueprintCallable, Category = "Geo Utils")
	static FVector GetUpVector(FVector CoordsOfUpVector, ACesiumGeoreference* CesiumGeoreference);
	
	static FMatrix GetLocalSpatialReferenceFrame(const FVector &LonLatHeightPos, ACesiumGeoreference* CesiumGeoreference);
	
	UFUNCTION(BlueprintCallable, Category = "Quat erion Util")
	static FQuat BuildQuatFromMatrix(FMatrix RotationMatrix);
	
	UFUNCTION(BlueprintCallable, Category = "Matrix Helper Tools")
	static FMatrix BuildMatrixFromVectors(FVector AxisX, FVector AxisY); 
	
	UFUNCTION(BlueprintCallable, Category = "Matrix Helper Tools")
	static FMatrix CalculateRotationMatrix(FMatrix SourceMatrix, FMatrix TargetMatrix);
	
	UFUNCTION()
	static FCalibratedData CalibrateAnchors(const FVector& AAnchorPos, const FVector& BAnchorPos, const FVector& DAnchorPos); 
};
