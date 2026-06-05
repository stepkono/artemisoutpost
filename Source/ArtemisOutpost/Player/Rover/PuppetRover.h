// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArtemisOutpost/Cesium/GeoRefsManager.h"
#include "GameFramework/Actor.h"
#include "PuppetRover.generated.h"

class AMasterRover;

UCLASS()
class ARTEMISOUTPOST_API APuppetRover : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	APuppetRover();
	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
	UFUNCTION(BlueprintCallable, Category = "Master Rover")
	void SetMaster(AMasterRover* MasterRover); 

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
protected:
	UPROPERTY(BlueprintReadOnly, Replicated)
	AMasterRover* Master; 

private: 
	UPROPERTY()
	AGeoRefsManager* GeoRefsManager; 
	
	UPROPERTY()
	float GeoRefScalingFactor; 
};
