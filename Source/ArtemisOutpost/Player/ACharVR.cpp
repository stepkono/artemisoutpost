// Fill out your copyright notice in the Description page of Project Settings.


#include "ACharVR.h"
#include "Cesium3DTileset.h"
#include "EngineUtils.h"


// Sets default values
ACharVR::ACharVR()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	bAlwaysRelevant = true;
}

// Called when the game starts or when spawned
void ACharVR::BeginPlay()
{
	Super::BeginPlay();
	
	const UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("ACharVR: Failed to get world.")); 
		return; 
	}
	
	for (const auto TileSet : TActorRange<ACesium3DTileset>(World))
	{
		if (TileSet->ActorHasTag(FName("DEFAULT_TILESET")))
		{
			VRTileSet = TileSet;
			break; 
		}
	}
	if (!VRTileSet)
	{
		UE_LOG(LogTemp, Error, TEXT("ACharVR: Failed to initialize VR Moon tileset.")); 
	}
}

// Called every frame
void ACharVR::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// Called to bind functionality to input
void ACharVR::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void ACharVR::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();
	
	UE_LOG(LogTemp, Log, TEXT("ACharVR: NotifyControllerChanged"));
	
	
	// Check locally 
	if (GetLocalRole() == ROLE_AutonomousProxy)
	{
		const AController* CurrentController = GetController();
		
		if (!VRTileSet)
		{
			UE_LOG(LogTemp, Error, TEXT("ACharVR: Tileset it not set."))
			return; 
		}
		
		// UNPOSSESSED
		if (CurrentController == nullptr)
		{
			UE_LOG(LogTemp, Log, TEXT("ACharVR: UNPOSSESSED"));
			//VRTileSet->SetActorHiddenInGame(true);
		}
		//POSSESSED
		else
		{
			UE_LOG(LogTemp, Log, TEXT("ACharVR: POSSESSED"));
			//VRTileSet->SetActorHiddenInGame(false);
		}		
	}
}

ACesium3DTileset* ACharVR::GetVRTileset()
{
	return VRTileSet;
}
