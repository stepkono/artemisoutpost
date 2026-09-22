// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MoonVisualisationLayer.h"
#include "ARMoonVisualisationLayer.generated.h"

/**
 * Beacons on the AR table moon. Shows cues from VR and R players; AR players' pointers are drawn as
 * lasers by the AR pawns themselves, so they are filtered out here.
 */
UCLASS(Blueprintable)
class ARTEMISOUTPOST_API AARMoonVisualisationLayer : public AMoonVisualisationLayer
{
	GENERATED_BODY()

protected:
	virtual ACesiumGeoreference* ResolveMoon(AGeoRefsManager* GeoRefs) const override;
	virtual FVector GeoToWorldOnMoon(AGeoRefsManager* GeoRefs, const FVector& LonLatHeight) const override;
	virtual bool ShouldShow(EPlayerContext SourceContext, EBeaconSource Source) const override;
};
