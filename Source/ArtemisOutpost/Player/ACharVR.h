// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Cesium3DTileset.h"
#include "CesiumGeoreference.h"
#include "GameFramework/Character.h"
#include "ACharVR.generated.h"

UCLASS()
class ARTEMISOUTPOST_API ACharVR : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ACharVR();
	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
	UFUNCTION(BlueprintCallable, Category = "VR Moon")
	ACesium3DTileset* GetVRTileset(); 

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	virtual void NotifyControllerChanged() override;
	
private: 
	UPROPERTY()
	ACesium3DTileset* VRTileSet;
};
