// Fill out your copyright notice in the Description page of Project Settings.


#include "CustomCesiumCameraManager.h"

ACustomCesiumCameraManager::ACustomCesiumCameraManager()
{
	// The base ACesiumCameraManager does NOT enable ticking, so without this our Tick() — which
	// repositions the proxy camera to follow the rover every frame — would never run. The camera
	// would stay frozen at the rover's SPAWN location, so Cesium would stream fine tiles/collision
	// only around spawn and progressively coarser ones as the rover drove away, dropping the rover
	// onto a crude flat collision hull below the visual surface. Enabling tick makes the proxy track
	// the rover so detail follows it everywhere.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void ACustomCesiumCameraManager::BeginPlay()
{
	Super::BeginPlay();

	// Cesium tilesets read GetAllCameras() from the default camera manager (the ACesiumCameraManager
	// tagged DEFAULT_CAMERAMANAGER in the PersistentLevel). If that isn't us, every camera we register
	// is ignored, so fail loudly.
	if (ACesiumCameraManager::GetDefaultCameraManager(GetWorld()) != this)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[CamMgr] %s is NOT the default camera manager — tilesets will NOT read our proxy cameras. "
			     "Ensure this actor has the 'DEFAULT_CAMERAMANAGER' tag in the PersistentLevel and no other tagged manager exists."),
			*GetName());
	}
}

void ACustomCesiumCameraManager::AddNewMasterRover(AMasterRover* InMasterRover)
{
	if (!InMasterRover)
	{
		UE_LOG(LogTemp, Error, TEXT("[CamMgr] AddNewMasterRover: InMasterRover is NULL — aborting (no camera registered)."));
		return;
	}

	if (!VRGeoRef)
	{
		UE_LOG(LogTemp, Error, TEXT("[CamMgr] AddNewMasterRover: VRGeoRef is null — aborting."));
		return;
	}

	USceneComponent* VirtualProxyCam = nullptr;
	const TArray<UActorComponent*> FoundComponents = InMasterRover->GetComponentsByTag(USceneComponent::StaticClass(), FName("VirtualCesiumCam"));
	if (FoundComponents.Num() > 0)
	{
		VirtualProxyCam = Cast<USceneComponent>(FoundComponents[0]);
	}
	if (!VirtualProxyCam)
	{
		UE_LOG(LogTemp, Error, TEXT("[CamMgr] AddNewMasterRover: no component tagged 'VirtualCesiumCam' on %s — aborting."), *GetNameSafe(InMasterRover));
		return;
	}

	FCesiumCamera CenterRenderCam;
	CenterRenderCam.ViewportSize = FVector2D(2048, 2048);
	CenterRenderCam.Location = VirtualProxyCam->GetComponentLocation();

	const FVector DownDirection = (VRGeoRef->GetActorLocation() - VirtualProxyCam->GetComponentLocation()).GetSafeNormal();
	CenterRenderCam.Rotation = DownDirection.Rotation();
	CenterRenderCam.FieldOfViewDegrees = 120.0;

	const int32 CameraID = AddCamera(CenterRenderCam);
	MasterRoversVirtualCams.Add(FVirtualCamera(CameraID, VirtualProxyCam, CenterRenderCam));
}

void ACustomCesiumCameraManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Keep each proxy camera glued to its rover's VirtualCesiumCam component, looking down at the moon
	// centre, so Cesium streams high-detail tiles/collision at the rover wherever it drives.
	for (FVirtualCamera& VirtualCamera : MasterRoversVirtualCams)
	{
		if (!VirtualCamera.VirtualProxy || !VRGeoRef || VirtualCamera.CameraID == -1) continue;

		VirtualCamera.CesiumCamera.Location = VirtualCamera.VirtualProxy->GetComponentLocation();

		const FVector DownDirection = (VRGeoRef->GetActorLocation() - VirtualCamera.VirtualProxy->GetComponentLocation()).GetSafeNormal();
		VirtualCamera.CesiumCamera.Rotation = DownDirection.Rotation();

		UpdateCamera(VirtualCamera.CameraID, VirtualCamera.CesiumCamera);
	}
}
