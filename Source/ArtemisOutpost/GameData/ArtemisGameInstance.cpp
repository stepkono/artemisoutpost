// Fill out your copyright notice in the Description page of Project Settings.


#include "ArtemisGameInstance.h"

bool UArtemisGameInstance::CheckForInitializedSpatialAnchors() const
{
	const bool bAnchorsInitialized = RawAnchors.AAnchorUUID.IsValidUUID()
		&& RawAnchors.BAnchorUUID.IsValidUUID()
		&& RawAnchors.CAnchorUUID.IsValidUUID()
		&& RawAnchors.DAnchorUUID.IsValidUUID();
	
	return bAnchorsInitialized; 
}


