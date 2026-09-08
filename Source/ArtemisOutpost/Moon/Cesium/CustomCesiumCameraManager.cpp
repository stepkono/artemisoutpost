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

void ACustomCesiumCameraManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Level teardown / travel: give every id back before we go away. The base manager's _cameras map
	// outlives our array, so skipping this would strand entries in it if this actor is ever reused
	// (PIE restart against a persistent world, seamless travel).
	for (const FVirtualCamera& VirtualCamera : MasterRoversVirtualCams)
	{
		UnregisterCamera(VirtualCamera);
	}
	MasterRoversVirtualCams.Empty();

	Super::EndPlay(EndPlayReason);
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
	if (FoundComponents.Num() <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[CamMgr] AddNewMasterRover: no component tagged 'VirtualCesiumCam' on %s — aborting."), *GetNameSafe(InMasterRover));
		return;
	}
	
	for (auto const FoundComponent : FoundComponents)
	{
		VirtualProxyCam = Cast<USceneComponent>(FoundComponent);
	
		if (!VirtualProxyCam)
		{
			UE_LOG(LogTemp, Error, TEXT("[CamMgr] AddNewMasterRover: Failed to Cast VirtualProxyCam"));
			continue;
		}
		
		FCesiumCamera CenterRenderCam;
		CenterRenderCam.ViewportSize = FVector2D(2048, 2048);
		CenterRenderCam.Location = VirtualProxyCam->GetComponentLocation();

		const FVector DownDirection = (VRGeoRef->GetActorLocation() - VirtualProxyCam->GetComponentLocation()).GetSafeNormal();
		CenterRenderCam.Rotation = DownDirection.Rotation();
		CenterRenderCam.FieldOfViewDegrees = 120.0;
		
		ECameraRole CameraRole;
		if (FoundComponent->ComponentHasTag(FName("RoverBelly")))
		{
			CameraRole = ECameraRole::ROVER_BELLY;
		}
		else
		{
			CameraRole = ECameraRole::OTHER;
		}
		
		// Assigned by name rather than by aggregate init, which silently rots if the struct's field
		// order ever changes.
		FVirtualCamera NewVirtualCamera;
		NewVirtualCamera.CameraID     = AddCamera(CenterRenderCam);
		NewVirtualCamera.VirtualProxy = VirtualProxyCam;
		NewVirtualCamera.CameraRole   = CameraRole;
		NewVirtualCamera.CesiumCamera = CenterRenderCam;

		MasterRoversVirtualCams.Add(NewVirtualCamera);
	}
}

void ACustomCesiumCameraManager::PruneDeadCameras()
{
	// Backwards so RemoveAtSwap cannot move an entry we have not visited yet into an index we already passed.
	for (int32 Index = MasterRoversVirtualCams.Num() - 1; Index >= 0; --Index)
	{
		const FVirtualCamera& VirtualCamera = MasterRoversVirtualCams[Index];

		// IsValid, NOT a null test, and that distinction is the whole point of this pass. Destroying a
		// rover marks its components as garbage immediately, but VirtualProxy is a raw UPROPERTY pointer
		// and is only cleared when a GC pass comes around, which can be seconds later. A null check
		// would keep reading GetComponentLocation() off a garbage component until then. IsValid goes
		// false the moment the object is marked, so a destroyed rover is unregistered on the next tick.
		//
		// A component cannot outlive its owning actor, so this covers a dead rover as well as a
		// component destroyed on its own.
		if (IsValid(VirtualCamera.VirtualProxy))
		{
			continue;
		}

		UE_LOG(LogTemp, Display,
			TEXT("[CamMgr] Pruning camera %d — its proxy component is gone (rover destroyed)."),
			VirtualCamera.CameraID);

		UnregisterCamera(VirtualCamera);
		MasterRoversVirtualCams.RemoveAtSwap(Index);
	}
}

void ACustomCesiumCameraManager::UnregisterCamera(const FVirtualCamera& VirtualCamera)
{
	if (VirtualCamera.CameraID == -1)
	{
		return;
	}

	// RemoveCamera is flagged deprecated in favour of the AdditionalCameras array, but AddCamera and
	// UpdateCamera (which this class already uses) sit on the same id-based path. Registration and
	// removal have to stay on one path together, so this moves to AdditionalCameras only when they do.
	if (!RemoveCamera(VirtualCamera.CameraID))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[CamMgr] RemoveCamera(%d) returned false — the base manager no longer holds that id."),
			VirtualCamera.CameraID);
	}
}

void ACustomCesiumCameraManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	PruneDeadCameras();
	
	for (FVirtualCamera& VirtualCamera : MasterRoversVirtualCams)
	{
		if (!IsValid(VirtualCamera.VirtualProxy) || VirtualCamera.CameraID == -1) continue;
		
		VirtualCamera.CesiumCamera.Location = VirtualCamera.VirtualProxy->GetComponentLocation();
		VirtualCamera.CesiumCamera.Rotation = VirtualCamera.VirtualProxy->GetComponentRotation();
		
		if (VirtualCamera.CameraRole == ECameraRole::ROVER_BELLY)
		{
			if (!VRGeoRef) continue;

			const FVector DownDirection = (VRGeoRef->GetActorLocation() - VirtualCamera.VirtualProxy->GetComponentLocation()).GetSafeNormal();
			VirtualCamera.CesiumCamera.Rotation = DownDirection.Rotation();	
		}
		
		UpdateCamera(VirtualCamera.CameraID, VirtualCamera.CesiumCamera);
	}
}
