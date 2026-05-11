// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OculusXRAnchorComponent.h"
#include "Engine/GameInstance.h"
#include "ArtemisGameInstance.generated.h"

USTRUCT(BlueprintType)
struct FCustomAnchors
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadWrite)
	FOculusXRUUID AAnchorUUID;
	
	UPROPERTY(BlueprintReadWrite)
	FOculusXRUUID BAnchorUUID;
	
	UPROPERTY(BlueprintReadWrite)
	FOculusXRUUID CAnchorUUID;
	
	UPROPERTY(BlueprintReadWrite)
	FOculusXRUUID DAnchorUUID; 
};

/**
 * 
 */
UCLASS()
class ARTEMISOUTPOST_API UArtemisGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spatial Anchors")
	FCustomAnchors RawAnchors;
};
