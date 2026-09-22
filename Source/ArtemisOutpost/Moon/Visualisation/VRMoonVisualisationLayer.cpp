// Fill out your copyright notice in the Description page of Project Settings.

#include "VRMoonVisualisationLayer.h"

#include "ArtemisOutpost/Moon/Cesium/GeoRefsManager.h"

ACesiumGeoreference* AVRMoonVisualisationLayer::ResolveMoon(AGeoRefsManager* GeoRefs) const
{
	return GeoRefs ? GeoRefs->GetVRMoon() : nullptr;
}

FVector AVRMoonVisualisationLayer::GeoToWorldOnMoon(AGeoRefsManager* GeoRefs, const FVector& LonLatHeight) const
{
	return GeoRefs ? GeoRefs->VRMoonCoordsToUECoords(LonLatHeight) : FVector::ZeroVector;
}

bool AVRMoonVisualisationLayer::ShouldShow(EPlayerContext SourceContext, EBeaconSource Source) const
{
	// VR -> VR is a laser on the character proxy, everything else lands here as a beacon.
	return SourceContext != EPlayerContext::VR;
}
