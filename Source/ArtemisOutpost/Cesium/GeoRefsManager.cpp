// Fill out your copyright notice in the Description page of Project Settings.


#include "GeoRefsManager.h"

#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"


// Sets default values
AGeoRefsManager::AGeoRefsManager()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void AGeoRefsManager::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void AGeoRefsManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

ACesiumGeoreference* AGeoRefsManager::GetVRMoon()
{
	return VRMoon;
}

ACesiumGeoreference* AGeoRefsManager::GetARMoon()
{
	return ARMoon;
}

FVector AGeoRefsManager::UECoordsToVRMoonCoords(FVector WorldCoords)
{
	const FVector LocalPos = VRMoon->GetActorTransform().InverseTransformPosition(WorldCoords);
	
	return VRMoon->TransformUnrealPositionToLongitudeLatitudeHeight(LocalPos);
}

FVector AGeoRefsManager::VRMoonCoordsToUECoords(FVector LonLatHeightCoords)
{
	const FVector LocalPos = VRMoon->TransformLongitudeLatitudeHeightPositionToUnreal(LonLatHeightCoords);
	
	return VRMoon->GetActorTransform().TransformPosition(LocalPos);
}

FVector AGeoRefsManager::ARMoonCoordsToUECoords(FVector LonLatHeightCoords)
{
	const FVector LocalPos = ARMoon->TransformLongitudeLatitudeHeightPositionToUnreal(LonLatHeightCoords);
	
	return ARMoon->GetActorTransform().TransformPosition(LocalPos);
}

FVector AGeoRefsManager::UECoordsToARMoonCoords(FVector WorldCoords)
{
	const FVector LocalPos = ARMoon->GetActorTransform().InverseTransformPosition(WorldCoords);
	
	return ARMoon->TransformUnrealPositionToLongitudeLatitudeHeight(LocalPos);
}
