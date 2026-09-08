// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArtemisOutpost/Moon/Cesium/GeoRefsManager.h"
#include "GameFramework/Actor.h"
#include "VRCharPuppet.generated.h"

class ACharVR;

UCLASS()
class ARTEMISOUTPOST_API AVRCharPuppet : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AVRCharPuppet();

	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
	UFUNCTION(BlueprintCallable, Category = "Master Char")
	void SetMaster(ACharVR* MasterVRChar);
	
	UFUNCTION(BlueprintCallable, Category = "Master Char")
	ACharVR* GetMaster(); 


protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	UPROPERTY(BlueprintReadOnly, Category = "Master")
	ACharVR* Master;
	
private: 
	UPROPERTY()
	AGeoRefsManager* GeoRefsManager; 
	
	UPROPERTY()
	float GeoRefScalingFactor; 
};
