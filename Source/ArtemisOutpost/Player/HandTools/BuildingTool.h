// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "HandToolBase.h"
#include "ArtemisOutpost/Minigame/Games/Habitat/Habitat.h"
#include "ArtemisOutpost/Minigame/Games/SignalTower/SignalTower.h"
#include "ArtemisOutpost/Minigame/Games/SolarPanel/SolarPanel.h"
#include "Engine/EngineTypes.h"
#include "ArtemisOutpost/Minigame/General/MinigameTypes.h"
#include "BuildingTool.generated.h"

class AMinigameActor;
class AGeoRefsManager;

/**
 * The building tool: aims a Niagara arc from the hand at the ground and places a building where it
 * lands. The per-frame arc/preview is authored in BP_BuildingTool (copied from the teleport-trace
 * demo). This C++ layer holds the chosen building type, validates the spot, and — because the tool
 * is client-owned (Set Owner on the Child Actor Component) and replicated — sends the spawn request
 * to the server itself, without routing through a separate controller.
 */
UCLASS(Abstract)
class ARTEMISOUTPOST_API ABuildingTool : public AHandToolBase
{
	GENERATED_BODY()

public:
	ABuildingTool();

	// Enter placement for a building type (called when a Build* tile is chosen). Activates the tool
	// (if needed) and fires OnPlacementBegan so the BP starts the arc.
	UFUNCTION(BlueprintCallable, Category = "Building Tool")
	void BeginPlacement(EOutpostBuildingType BuildingType);

	UFUNCTION(BlueprintPure, Category = "Building Tool")
	EOutpostBuildingType GetCurrentBuildingType() const { return CurrentBuildingType; }

	// Local validity check for feedback while aiming (call on trigger, NOT every tick). Deliberately
	// does NOT line-trace Cesium tiles — that crashes on a null-RHI server (project memory); it only
	// checks the surface normal (slope) and proximity to existing buildings.
	UFUNCTION(BlueprintCallable, Category = "Building Tool")
	bool CanPlaceBuildingAt(FVector Location, FVector SurfaceNormal, FText& OutReason) const;

	// Confirm: validate + request the server spawn. Returns false (no request) if invalid. Does NOT
	// holster — the player keeps the tool to place again.
	UFUNCTION(BlueprintCallable, Category = "Building Tool")
	bool TryBuildAtPlacement(FVector PlacementLocation);

	// AHandToolBase — trigger DOWN while this is the active tool: build at the current aim point.
	// The BP arc writes ProposedBuildLocation each tick; this just confirms there.
	virtual void ExecuteAction() override;

	// Geodetic "up" (moon surface normal) at a world position, via the VR-moon georeference.
	UFUNCTION(BlueprintPure, Category = "Building Tool")
	FVector GetSurfaceUp(FVector WorldPos) const;

	// Predict Projectile Path, rebuilt with gravity toward the MOON surface (−surface normal) instead
	// of world −Z, so the arc always bends to the ground wherever you stand on the globe. Call this in
	// BP in place of the engine node. Returns true if the arc hit something (OutHit = landing point).
	// Inputs mirror the engine node; leave the trailing values at their defaults unless tuning.
	UFUNCTION(BlueprintCallable, Category = "Building Tool", meta = (AutoCreateRefTerm = "ObjectTypes,ActorsToIgnore"))
	bool PredictArcOnSurface(FVector StartPos, FVector LaunchVelocity,
		const TArray<TEnumAsByte<EObjectTypeQuery>>& ObjectTypes,
		const TArray<AActor*>& ActorsToIgnore,
		TArray<FVector>& OutPathPositions, FHitResult& OutHit,
		bool bInertia = false,
		float GravityMagnitude = 980.0f, float ProjectileRadius = 10.0f,
		float SimFrequency = 15.0f, float MaxSimTime = 6.0f,
		float MaxLaunchAngleDeg = 55.0f, bool bTraceComplex = false);

protected:
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Building Tool Trace")
	void TogglePlacementVisual(bool bShowVisual); 
	
	// Client -> server spawn request. The server re-validates (authority) and spawns the mapped
	// AMinigameActor subclass at the transform.
	UFUNCTION(Server, Reliable, Category = "Building Tool")
	void ServerPlaceBuilding(EOutpostBuildingType BuildingType, FTransform PlacementTransform);

	UPROPERTY(BlueprintReadOnly, Category = "Building Tool")
	bool bPlacementBegan = false; 
	
	UPROPERTY(BlueprintReadOnly, Category = "Building Tool")
	EOutpostBuildingType CurrentBuildingType = EOutpostBuildingType::Habitat;

	// Current aim/landing point, written by the BP arc each tick (Out Hit -> Impact Point). ExecuteAction
	// (trigger) builds here — so the confirm logic lives in C++ while the arc stays in BP.
	UPROPERTY(BlueprintReadWrite, Category = "Building Tool")
	FVector ProposedBuildLocation = FVector::ZeroVector;

	// Building type -> spawned actor class. Assign in BP_BuildingTool (Antenna -> BP_SignalTower;
	// Habitat / SolarPanel -> new AMinigameActor children). Adding a building = one row here.
	UPROPERTY(EditDefaultsOnly, Category = "Building Tool")
	TMap<EOutpostBuildingType, TSubclassOf<AMinigameActor>> BuildingClasses;

	// Max ground slope (deg, surface normal vs. the pawn's up / geodetic up) a building tolerates.
	UPROPERTY(EditDefaultsOnly, Category = "Building Tool", meta = (ClampMin = "0.0", ClampMax = "89.0"))
	float MaxPlacementSlopeDegrees = 20.0f;

	// Minimum centre-to-centre distance (cm) to any existing building.
	UPROPERTY(EditDefaultsOnly, Category = "Building Tool", meta = (ClampMin = "0.0"))
	float MinBuildingSpacing = 200.0f;
	
	// Inertia catch-up speed at the START of the beam (near the hand). High = snappy/near-instant.
	UPROPERTY(EditDefaultsOnly, Category = "Building Tool|Inertia", meta = (ClampMin = "0.5"))
	float InertiaSpeedStart = 30.0f;

	// Inertia catch-up speed at the END of the beam (the tip). Low = laggy, so the curve trails/bends.
	UPROPERTY(EditDefaultsOnly, Category = "Building Tool|Inertia", meta = (ClampMin = "0.5"))
	float InertiaSpeedEnd = 4.0f;

	// Ease-in exponent for the lag ramp (1 = linear, higher = keeps the START snappy longer so the lag
	// grows smoothly and there is no hard kink near the muzzle). 2–3 is a good range.
	UPROPERTY(EditDefaultsOnly, Category = "Building Tool|Inertia", meta = (ClampMin = "1.0"))
	float InertiaLagFalloff = 2.5f;

	// Points the beam is resampled to while inertia is on (so each point can lag independently).
	UPROPERTY(EditDefaultsOnly, Category = "Building Tool|Inertia", meta = (ClampMin = "2"))
	int32 InertiaPointCount = 32;
	
private:
	// VR-moon georeference source, found in BeginPlay. Used to compute the surface normal.
	UPROPERTY(Transient)
	TObjectPtr<AGeoRefsManager> GeoRefsManager;

	// Persistent smoothed beam for the inertia effect.
	UPROPERTY(Transient)
	TArray<FVector> SmoothedPath;
};
