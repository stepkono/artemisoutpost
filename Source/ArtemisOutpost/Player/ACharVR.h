// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Cesium3DTileset.h"
#include "CesiumGeoreference.h"
#include "ArtemisOutpost/Cesium/GeoRefsManager.h"
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
	
	UFUNCTION(BlueprintCallable, Category = "GeoRefsManager")
	void SetGeoRefsManager(AGeoRefsManager* InManager);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	virtual void NotifyControllerChanged() override;
	
	UPROPERTY(BlueprintReadOnly, Category = "GeoRefsManager")
	AGeoRefsManager* GeoRefsManager;

private:
	UPROPERTY()
	ACesium3DTileset* VRTileSet;

	// Accumulates DeltaTime for the throttled client-side VR-moon collision probe in Tick.
	float DebugProbeAccumulator = 0.0f;
};
