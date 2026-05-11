// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

USTRUCT()
struct FAnchorsPositions
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

/**
 * 
 */
class ARTEMISOUTPOST_API DataTypes
{
public:
	DataTypes();
	~DataTypes();
};
