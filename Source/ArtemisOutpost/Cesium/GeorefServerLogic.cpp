// Fill out your copyright notice in the Description page of Project Settings.


#include "GeorefServerLogic.h"

#include "Cesium3DTileset.h"
#include "ArtemisOutpost/Miscellaneous/NetUtils.h"


// Sets default values for this component's properties
UGeorefServerLogic::UGeorefServerLogic()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// ...
}


// Called when the game starts
void UGeorefServerLogic::BeginPlay()
{
	Super::BeginPlay();
	
	if (!ArtemisNet::IsServerHost(GetNetMode()))
	{
		return;	
	}

	if (!VRTileSet)
	{
		UE_LOG(LogTemp, Error, TEXT("GeoRefServerLogic: VRTileSet is null"));
		return;
	}
	
	VRTileSet->EnableFogCulling = false; 
	VRTileSet->EnableFrustumCulling = false; 
}


// Called every frame
void UGeorefServerLogic::TickComponent(float DeltaTime, ELevelTick TickType,
                                       FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

