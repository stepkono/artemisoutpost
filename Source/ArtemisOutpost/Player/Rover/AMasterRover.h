// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "ArtemisOutpost/Cesium/GeoRefsManager.h"
#include "AMasterRover.generated.h"

class APuppetRover;

/**
 * 
 */
UCLASS()
class ARTEMISOUTPOST_API AMasterRover : public AWheeledVehiclePawn
{
	GENERATED_BODY()
	
public:
	AMasterRover();
	
	virtual void BeginPlay() override;
	
	UFUNCTION()
	FVector GetLocalPos_UE() const; 
	
	UFUNCTION()
	FQuat GetAbsoluteOrientation(); 
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	UFUNCTION(BlueprintCallable, Category = "Puppet Rover")
	APuppetRover* GetPuppetRover(); 
	
private:
	UPROPERTY(Replicated)
	APuppetRover* PuppetRover;
	
	UPROPERTY()
	AGeoRefsManager* GeoRefsManager;
	
	UPROPERTY()
	FVector StartLocalPosition_UE; 
};
