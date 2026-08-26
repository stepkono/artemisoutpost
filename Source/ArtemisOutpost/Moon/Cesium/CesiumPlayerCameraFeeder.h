// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CesiumCameraManager.h"
#include "CesiumPlayerCameraFeeder.generated.h"

/**
 * Custom CesiumCameraManager that drives tile LOD from the local player pawn.
 *
 * Problem:
 *   The default ACesiumCameraManager relies on PlayerController cameras.
 *   In XR / custom-pawn setups that chain is broken, so the tileset never
 *   sees the player's viewpoint and streams everything at minimum LOD.
 *
 * Solution:
 *   This subclass overrides Tick to read the pawn's eye transform each frame
 *   and write it into AdditionalCameras[0].  GetAllCameras() in the base class
 *   already merges AdditionalCameras into the list that every Cesium3DTileset
 *   queries, so no tileset changes are needed.
 *
 * Setup in editor:
 *   1. Delete (or leave absent) any existing CesiumCameraManager in the level.
 *   2. Place ONE instance of this actor in the persistent level.
 *   3. In the Details panel → Tags array, add the tag:  DEFAULT_CAMERAMANAGER
 *      This makes GetDefaultCameraManager() return this actor instead of
 *      spawning a plain ACesiumCameraManager.
 *   4. Optionally assign TrackedPawn.  If left null, BeginPlay and Tick will
 *      auto-resolve Player 0's pawn (safe for late-spawning pawns).
 */
UCLASS(Blueprintable, BlueprintType, meta = (DisplayName = "Cesium Player Camera Manager"))
class ARTEMISOUTPOST_API ACesiumPlayerCameraFeeder : public ACesiumCameraManager
{
	GENERATED_BODY()

public:
	ACesiumPlayerCameraFeeder();

	// -------------------------------------------------------------------------
	// Configuration
	// -------------------------------------------------------------------------

	/**
	 * The pawn whose eye transform is forwarded to the Cesium tile system.
	 * Leave null to auto-detect the first local player's pawn each frame.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cesium|Player Camera")
	TObjectPtr<APawn> TrackedPawn;

	/**
	 * Horizontal field of view in degrees.
	 * Should match your XR camera / HMD eye FOV (default 90°).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cesium|Player Camera",
		meta = (ClampMin = "1.0", ClampMax = "179.0"))
	float FieldOfViewDegrees = 90.f;

	// -------------------------------------------------------------------------
	// ACesiumCameraManager / AActor overrides
	// -------------------------------------------------------------------------
	virtual void BeginPlay() override;

	/**
	 * Calls Super::Tick first (preserves base-class camera gathering),
	 * then overwrites AdditionalCameras[0] with the current pawn eye transform.
	 */
	virtual void Tick(float DeltaTime) override;

private:
	/** Returns TrackedPawn if valid, otherwise Player 0's pawn. */
	APawn* ResolvePawn() const;

	/** Returns the current viewport size, with a sensible fallback. */
	static FVector2D GetViewportSize();
};
