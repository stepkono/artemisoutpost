// Fill out your copyright notice in the Description page of Project Settings.


#include "AMasterRover.h"
#include "EngineUtils.h"
#include "ArtemisOutpost/Cesium/GeoRefsManager.h"

void AMasterRover::BeginPlay()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("AMasterRover: Failed to get World. Aborting..."));
		return; 
	}
	
	for (TActorIterator<AGeoRefsManager> It(World); It; ++It)
	{
		if (AGeoRefsManager* Manager = *It)
		{
			GeoRefsManager = Manager;
			break; 
		}
	}
	
	if (!GeoRefsManager)
	{
		UE_LOG(LogTemp, Error, TEXT("AMasterRover: Failed to get GeoRefsManager."));
		return; 
	}
	
	// The server side should spawn the puppet
	if (HasAuthority())
	{
		FVector MasterSpawnCoords_UE  = this->GetActorLocation();
		FVector MasterSpawnCoords_Geo = GeoRefsManager->UECoordsToVRMoonCoords(MasterSpawnCoords_UE); 
		
		SpawnAndAssignPuppet(MasterSpawnCoords_Geo);
	}
}

void AMasterRover::SpawnAndAssignPuppet(FVector& GeoSpawnCoords)
{
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn; 
	
	PuppetRover = GetWorld()->SpawnActor<APuppetRover>(SpawnParams);
}

void AMasterRover::OnRep_SetPuppetRoverLocalLocation()
{
	FVector MasterSpawnCoords_UE  = this->GetActorLocation();
	FVector MasterSpawnCoords_Geo = GeoRefsManager->UECoordsToVRMoonCoords(MasterSpawnCoords_UE); 
	
	const FVector PuppetSpawnCoords_UE  = GeoRefsManager->ARMoonCoordsToUECoords(MasterSpawnCoords_Geo); 
	const FRotator PuppetRotation = this->GetActorRotation();
	const FTransform PuppetTransform = FTransform(PuppetRotation, PuppetSpawnCoords_UE);
	
	PuppetRover->SetActorTransform(PuppetTransform);
}
