// Fill out your copyright notice in the Description page of Project Settings.


#include "CustomCesiumCameraManager.h"

ACustomCesiumCameraManager::ACustomCesiumCameraManager()
{
	// The base ACesiumCameraManager does NOT enable ticking, and this subclass had no constructor,
	// so our Tick() — which repositions the proxy camera to follow the rover every frame — never
	// ran. AddNewMasterRover registered the camera once at the rover's SPAWN location and it stayed
	// frozen there, so Cesium streamed fine tiles/collision only around spawn and progressively
	// coarser ones as the rover drove away (measured collision boundsR 3k -> 96k), dropping the
	// rover onto a crude flat collision hull below the detailed visual surface (the "submerged" bug).
	// Enabling tick makes the proxy track the rover so detail follows it everywhere.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void ACustomCesiumCameraManager::BeginPlay()
{
	Super::BeginPlay();

	// Diagnostic: are WE the manager the tilesets actually read from?
	// Cesium tilesets call GetDefaultCameraManager() and read GetAllCameras() from it.
	// "Default" = an ACesiumCameraManager(-derived) actor in the PersistentLevel tagged
	// "DEFAULT_CAMERAMANAGER". If that isn't us, every camera we register is ignored.
	const ACesiumCameraManager* DefaultMgr = ACesiumCameraManager::GetDefaultCameraManager(GetWorld());
	const bool bHasTag   = ActorHasTag(FName("DEFAULT_CAMERAMANAGER"));
	const bool bIsDefault = (DefaultMgr == this);
	const bool bInPersistent = GetLevel() == GetWorld()->PersistentLevel;

	UE_LOG(LogTemp, Warning,
		TEXT("[CamMgr] this=%s | IsDefault=%d | HasDefaultTag=%d | InPersistentLevel=%d | DefaultMgr=%s"),
		*GetName(), bIsDefault, bHasTag, bInPersistent, *GetNameSafe(DefaultMgr));

	if (!bIsDefault)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[CamMgr] NOT the default camera manager — tilesets will NOT read our proxy cameras. "
			     "Add the 'DEFAULT_CAMERAMANAGER' tag to this actor in the PersistentLevel (and ensure no other tagged manager exists)."));
	}
}

void ACustomCesiumCameraManager::AddNewMasterRover(AMasterRover* InMasterRover)
{
	// Unconditional entry log: confirms the call reaches C++, on WHICH manager instance,
	// whether that instance is the default Cesium reads from, and the arg/VRGeoRef state.
	const bool bIsDefault = (ACesiumCameraManager::GetDefaultCameraManager(GetWorld()) == this);
	UE_LOG(LogTemp, Warning,
		TEXT("[CamMgr] AddNewMasterRover CALLED | this=%s | IsDefault=%d | InMasterRover=%s | VRGeoRef=%s"),
		*GetName(), bIsDefault ? 1 : 0, *GetNameSafe(InMasterRover), *GetNameSafe(VRGeoRef));

	if (!InMasterRover)
	{
		UE_LOG(LogTemp, Error, TEXT("[CamMgr] AddNewMasterRover: InMasterRover is NULL — aborting (no camera registered)."));
		return;
	}

	if (!VRGeoRef)
	{
		UE_LOG(LogTemp, Error, TEXT("CustomCesiumCameraManager: VRGeoRef is null. Aborting AddNewMasterRover()."));
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
		UE_LOG(LogTemp, Error, TEXT("CustomCesiumCameraManager: VirtualProxyCam or GeoRef is null. Ensure a component has 'VirtualCesiumCam' tag and GeoRef is assigned."));
		return;
	}

	FCesiumCamera CenterRenderCam; 
	// TODO: this might be smaller maybe
	CenterRenderCam.ViewportSize = FVector2D(2048, 2048);
	CenterRenderCam.Location = VirtualProxyCam->GetComponentLocation();
	
	const FVector DownDirection = (VRGeoRef->GetActorLocation() - VirtualProxyCam->GetComponentLocation()).GetSafeNormal();
	CenterRenderCam.Rotation = DownDirection.Rotation();
	CenterRenderCam.FieldOfViewDegrees = 120.0;

	const int32 CameraID = AddCamera(CenterRenderCam);
	const FVirtualCamera VirtualCam(CameraID, VirtualProxyCam, CenterRenderCam);

	MasterRoversVirtualCams.Add(VirtualCam);

	UE_LOG(LogTemp, Warning,
		TEXT("[CamMgr] AddNewMasterRover OK | CameraID=%d | ProxyLoc=%s | Rot=%s | GetAllCameras().size()=%d"),
		CameraID, *CenterRenderCam.Location.ToString(), *CenterRenderCam.Rotation.ToString(),
		(int32)GetAllCameras().size());
}

void ACustomCesiumCameraManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	for (FVirtualCamera& VirtualCamera : MasterRoversVirtualCams)
	{
		if (!VirtualCamera.VirtualProxy || !VRGeoRef || VirtualCamera.CameraID == -1) continue;

		VirtualCamera.CesiumCamera.Location = VirtualCamera.VirtualProxy->GetComponentLocation();

		const FVector DownDirection = (VRGeoRef->GetActorLocation() - VirtualCamera.VirtualProxy->GetComponentLocation()).GetSafeNormal();
		VirtualCamera.CesiumCamera.Rotation = DownDirection.Rotation();

		UpdateCamera(VirtualCamera.CameraID, VirtualCamera.CesiumCamera);
	}

	// Throttled diagnostic (~every 5s): confirm the tileset will see our cameras and
	// that the proxy actually tracks the rover. [BasketA] Logs UNCONDITIONALLY (even when 0)
	// and tags SERVER/CLIENT: if RegisteredProxies=0 on the CLIENT, nothing is driving VR-moon
	// tile/collision streaming there, which is the suspected root of the client fall-through.
	DebugLogAccumulator += DeltaTime;
	if (DebugLogAccumulator >= 5.0f)
	{
		DebugLogAccumulator = 0.0f;
		const TCHAR* Net = (GetNetMode() == NM_Client) ? TEXT("CLIENT") : TEXT("SERVER");
		if (MasterRoversVirtualCams.Num() > 0)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[CamMgr][%s] Tick | RegisteredProxies=%d | GetAllCameras().size()=%d | Cam0Loc=%s | Cam0Rot=%s"),
				Net, MasterRoversVirtualCams.Num(), (int32)GetAllCameras().size(),
				*MasterRoversVirtualCams[0].CesiumCamera.Location.ToString(),
				*MasterRoversVirtualCams[0].CesiumCamera.Rotation.ToString());
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[CamMgr][%s] Tick | RegisteredProxies=0 (nothing driving VR-moon streaming on this instance) | GetAllCameras().size()=%d"),
				Net, (int32)GetAllCameras().size());
		}
	}
}