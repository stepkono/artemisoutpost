// Fill out your copyright notice in the Description page of Project Settings.


#include "CustomCesiumCameraManager.h"

void ACustomCesiumCameraManager::BeginPlay()
{
	Super::BeginPlay();
}

void ACustomCesiumCameraManager::AddNewMasterRover(AMasterRover* InMasterRover)
{
	if (!InMasterRover) return;
	
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
}