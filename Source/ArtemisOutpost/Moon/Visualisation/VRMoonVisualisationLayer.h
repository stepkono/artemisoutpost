// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MoonVisualisationLayer.h"
#include "VRMoonVisualisationLayer.generated.h"

/**
 * Beacons on the VR moon. Shows cues from AR and R players; VR players' pointers are drawn as lasers
 * by their VR character proxies, so they are filtered out here.
 */
UCLASS(Blueprintable)
class ARTEMISOUTPOST_API AVRMoonVisualisationLayer : public AMoonVisualisationLayer
{
	GENERATED_BODY()

protected:
	virtual ACesiumGeoreference* ResolveMoon(AGeoRefsManager* GeoRefs) const override;
	virtual FVector GeoToWorldOnMoon(AGeoRefsManager* GeoRefs, const FVector& LonLatHeight) const override;
	virtual bool ShouldShow(EPlayerContext SourceContext, EBeaconSource Source) const override;
};
