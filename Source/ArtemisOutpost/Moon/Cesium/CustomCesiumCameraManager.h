// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CesiumCameraManager.h"
#include "ArtemisOutpost/Player/Rover/AMasterRover.h"
#include "CustomCesiumCameraManager.generated.h"

USTRUCT()
struct FVirtualCamera
{
	GENERATED_BODY()
	
	UPROPERTY()
	int32 CameraID = -1; 
	
	UPROPERTY()
	USceneComponent* VirtualProxy = nullptr;
	
	UPROPERTY()
	FCesiumCamera CesiumCamera;
};

UCLASS()
class ARTEMISOUTPOST_API ACustomCesiumCameraManager : public ACesiumCameraManager
{
	GENERATED_BODY()
	
public:
	ACustomCesiumCameraManager();

	UFUNCTION(BLueprintCallable, Category="Master Rover")
	void AddNewMasterRover(AMasterRover* InMasterRover);
	
protected: 
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

private:
	UPROPERTY(EditAnywhere, Category="VR GeoReference")
	ACesiumGeoreference* VRGeoRef = nullptr;

	UPROPERTY()
	TArray<FVirtualCamera> MasterRoversVirtualCams;
};