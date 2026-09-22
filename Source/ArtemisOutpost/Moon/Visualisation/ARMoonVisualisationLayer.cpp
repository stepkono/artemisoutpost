// Fill out your copyright notice in the Description page of Project Settings.

#include "ARMoonVisualisationLayer.h"

#include "ArtemisOutpost/Moon/Cesium/GeoRefsManager.h"

ACesiumGeoreference* AARMoonVisualisationLayer::ResolveMoon(AGeoRefsManager* GeoRefs) const
{
	return GeoRefs ? GeoRefs->GetARMoon() : nullptr;
}

FVector AARMoonVisualisationLayer::GeoToWorldOnMoon(AGeoRefsManager* GeoRefs, const FVector& LonLatHeight) const
{
	return GeoRefs ? GeoRefs->ARMoonCoordsToUECoords(LonLatHeight) : FVector::ZeroVector;
}

bool AARMoonVisualisationLayer::ShouldShow(EPlayerContext SourceContext, EBeaconSource Source) const
{
	// AR -> AR is a laser on the pawn, everything else lands here as a beacon.
	return SourceContext != EPlayerContext::AR;
}
