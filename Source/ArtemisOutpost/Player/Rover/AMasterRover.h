// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PuppetRover.h"
#include "WheeledVehiclePawn.h"
#include "ArtemisOutpost/Cesium/GeoRefsManager.h"
#include "AMasterRover.generated.h"

/**
 * 
 */
UCLASS()
class ARTEMISOUTPOST_API AMasterRover : public AWheeledVehiclePawn
{
	GENERATED_BODY()
	
public:
	virtual void BeginPlay() override;
	
private: 
	UFUNCTION()
	void SpawnAndAssignPuppet(FVector& GeoSpawnCoords); 
	
	void OnRep_SetPuppetRoverLocalLocation(); 

private:
	UPROPERTY(ReplicatedUsing=OnPuppetRoverCreated)
	APuppetRover* PuppetRover;
	
	UPROPERTY()
	AGeoRefsManager* GeoRefsManager;
};
