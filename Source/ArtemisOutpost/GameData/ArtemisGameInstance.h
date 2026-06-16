// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OculusXRAnchorComponent.h"
#include "Engine/GameInstance.h"
#include "ArtemisGameInstance.generated.h"

USTRUCT(BlueprintType)
struct FOrderedAnchors
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
	UFUNCTION()
	bool CheckForInitializedSpatialAnchors() const; 
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spatial Anchors")
	FOrderedAnchors RawAnchors;

	/** Group UUID generated for the current share session. Set by the AnchorsManagerSubsystem
	 *  when the authoritative client shares anchors, then replicated to clients via the GameState. */
	UPROPERTY(BlueprintReadWrite, Category="Spatial Anchors")
	FOculusXRUUID SharingGroupUUID;
};
