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
	
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("GeoRefManager: Failed to get World. Aborting..."));
		return; 
	}
	
	for (auto GeoRef : TActorRange<ACesiumGeoreference>(World))
	{
		if (GeoRef->ActorHasTag(FName("AR_GEOREF")))
		{
			ARMoon = GeoRef;
			break; 
		}
	}
	
	for (auto GeoRef : TActorRange<ACesiumGeoreference>(World))
	{
		if (GeoRef->ActorHasTag(FName("DEFAULT_GEOREFERENCE")))
		{
			VRMoon = GeoRef;
			break; 
		}
	}
	
	if (!(VRMoon && ARMoon))
	{
		UE_LOG(LogTemp, Error, TEXT("GeoRefsManager: Failed to initialize moon."));
		return; 
	}
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

FVector AGeoRefsManager::UECoordsToVRMoonCoords(FVector& WorldCoords)
{
	return FVector(0);
}

FVector AGeoRefsManager::VRMoonCoordsToUECoords(FVector& LatLonHeightCoords)
{
	return FVector(0);
}

FVector AGeoRefsManager::ARMoonCoordsToUECoords(FVector& LatLonHeightCoords)
{
	return FVector(0);
}

FVector AGeoRefsManager::UECoordsToARMoonCoords(FVector WorldCoords)
{
	return FVector(0);
}
