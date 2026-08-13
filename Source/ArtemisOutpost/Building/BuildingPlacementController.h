// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BuildingTypes.h"
#include "BuildingPlacementController.generated.h"

class AMinigameActor;

/**
 * Per-player building-placement controller, attached to APawnController (its own component so the
 * controller stays lean, mirroring UMinigamePlayerController). Two jobs:
 *   1) Transport / authority: ServerPlaceBuilding is a Server RPC that must originate on the
 *      client-owned PlayerController — the spawned buildings are server-owned AMinigameActors, so a
 *      client can't spawn them directly. This component is replicated so the RPC routes correctly.
 *   2) Validation: CanPlaceBuildingAt is the single source of truth, called on the client (trigger
 *      feedback) AND on the server (authority) before spawning.
 */
UCLASS(ClassGroup = (Building), meta = (BlueprintSpawnableComponent))
class ARTEMISOUTPOST_API UBuildingPlacementController : public UActorComponent
{
	GENERATED_BODY()

public:
	UBuildingPlacementController();

	// Client -> server request to place a building. The server re-validates (authority) and spawns
	// the mapped AMinigameActor subclass at the transform.
	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Building")
	void ServerPlaceBuilding(EOutpostBuildingType Type, FTransform PlacementTransform);

	// Shared placement validity check. Called on the CLIENT when the player pulls the trigger (for
	// immediate feedback) AND on the SERVER before spawning. Deliberately does NOT line-trace Cesium
	// tiles — that crashes on a null-RHI server (see project memory); it only checks the already-known
	// surface normal (slope) and proximity to existing buildings.
	UFUNCTION(BlueprintCallable, Category = "Building")
	bool CanPlaceBuildingAt(EOutpostBuildingType Type, FVector Location, FVector SurfaceNormal, FText& OutReason) const;

protected:
	// Building type -> spawned actor class. Assign on the BuildingPlacementController in BP_PawnController
	// (Antenna -> BP_SignalTower; Habitat / SolarPanel -> new AMinigameActor children). Adding a
	// building = one row here.
	UPROPERTY(EditDefaultsOnly, Category = "Building")
	TMap<EOutpostBuildingType, TSubclassOf<AMinigameActor>> BuildingClasses;

	// Max ground slope (deg, surface normal vs. the pawn's up / geodetic up) a building tolerates.
	UPROPERTY(EditDefaultsOnly, Category = "Building", meta = (ClampMin = "0.0", ClampMax = "89.0"))
	float MaxPlacementSlopeDegrees = 20.0f;

	// Minimum centre-to-centre distance (cm) to any existing building.
	UPROPERTY(EditDefaultsOnly, Category = "Building", meta = (ClampMin = "0.0"))
	float MinBuildingSpacing = 200.0f;

private:
	// The pawn currently possessed by the owning controller (reference "up" for the slope check).
	const APawn* GetOwningPawn() const;
};
