// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DataTypes.generated.h"

USTRUCT()
struct FOrderedAnchorsPositions
{
	GENERATED_BODY()
	
	UPROPERTY()
	FVector AAnchorPos; 
	
	UPROPERTY()
	FVector BAnchorPos;
	
	UPROPERTY()
	FVector CAnchorPos;
	
	UPROPERTY()
	FVector DAnchorPos;
};

USTRUCT()
struct FAnchorsActors
{
	GENERATED_BODY()
	
	UPROPERTY()
	AActor* AAnchor;
	
	UPROPERTY()
	AActor* BAnchor;
	
	UPROPERTY()
	AActor* CAnchor;
	
	UPROPERTY()
	AActor* DAnchor;
};

USTRUCT(BlueprintType)
struct FCoordinates
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadWrite)
	float Latitude = 0;
	
	UPROPERTY(BlueprintReadWrite)
	float Longitude = 0; 
	
	UPROPERTY(BlueprintReadWrite)
	float Height = 0; 
};

USTRUCT(BlueprintType)
struct FMapBaseCoordinates
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadWrite)
	FCoordinates UpLeft;
	
	UPROPERTY(BlueprintReadWrite)
	FCoordinates BottomLeft;
	
	UPROPERTY(BlueprintReadWrite)
	FCoordinates BottomRight;
	
	UPROPERTY(BlueprintReadWrite)
	FCoordinates Origin;
	
	UPROPERTY(BlueprintReadWrite)
	float TerrainElevation; 
};

USTRUCT(BlueprintType)
struct FControlCommand
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	float Accelerator = 0.0f;

	UPROPERTY(BlueprintReadWrite)
	float Steering = 0.0f;
};

USTRUCT()
struct FCalibratedData
{
	GENERATED_BODY()
	
	UPROPERTY()
	FVector AAnchorPos;
	
	UPROPERTY()
	FVector BAnchorPos;
	
	UPROPERTY()
	FVector CAnchorPos;
	
	UPROPERTY()
	FVector DAnchorPos;
	
	UPROPERTY()
	FVector PlaneCenter; 
	
	UPROPERTY()
	FVector PlaneNormal;
};

UENUM(BlueprintType)
enum EXRMode
{
	VR UMETA(DisplayName = "VR"),
	AR UMETA(DisplayName = "AR"),
};

/**
 * 
 */
class ARTEMISOUTPOST_API DataTypes
{
public:
	DataTypes();
	~DataTypes();
};
