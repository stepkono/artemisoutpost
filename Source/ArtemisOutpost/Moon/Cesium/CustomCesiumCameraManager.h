// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CesiumCameraManager.h"
#include "ArtemisOutpost/Player/Rover/AMasterRover.h"
#include "CustomCesiumCameraManager.generated.h"

UENUM()
enum ECameraRole
{
	ROVER_BELLY UMETA(DisplayName="Rover Belly"),
	OTHER UMETA(DisplayName="Rover VR Preview"),
};

USTRUCT()
struct FVirtualCamera
{
	GENERATED_BODY()
	
	UPROPERTY()
	int32 CameraID = -1; 
	
	UPROPERTY()
	USceneComponent* VirtualProxy = nullptr;

	UPROPERTY()
	TEnumAsByte<ECameraRole> CameraRole;
	
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

	// NOTE: there is deliberately no RemoveMasterRover. Unregistering is driven entirely by
	// PruneDeadCameras() in Tick, so no despawn path has to remember to call anything — client
	// disconnect, level travel, streaming-level unload and plain Destroy() are all covered by the same
	// check. Add an explicit removal only when something needs to stop a rover's streaming while the
	// rover is still ALIVE (e.g. parking it out of play), which nothing does today.

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;

private:
	// Drops entries whose proxy component has died, unregistering their Cesium camera. Runs at the top
	// of Tick so the update loop below only ever touches live components. This is the ONLY
	// unregistration path for rovers, by design (see the note above AddNewMasterRover).
	void PruneDeadCameras();

	// Hands one camera id back to the base manager. Does NOT touch MasterRoversVirtualCams — callers
	// own the bookkeeping, so this stays safe to use while iterating backwards over the array.
	void UnregisterCamera(const FVirtualCamera& VirtualCamera);

	UPROPERTY(EditAnywhere, Category="VR GeoReference")
	ACesiumGeoreference* VRGeoRef = nullptr;

	UPROPERTY()
	TArray<FVirtualCamera> MasterRoversVirtualCams;
};