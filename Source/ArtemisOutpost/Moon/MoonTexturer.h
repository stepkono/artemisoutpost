// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MoonTexturer.generated.h"

class UMaterialInstance;
class UMaterialInstanceDynamic;
class UMaterialParameterCollection;

/**
 * Lives on a Cesium tileset actor. Owns the fog-of-war data texture and the
 * dynamic material instance, and applies incoming scan data to them.
 *
 * It knows nothing about game logic: MoonDataManager computes the pixel data
 * and pushes it here via ApplyScanData().
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), Blueprintable)
class ARTEMISOUTPOST_API UMoonTexturer : public UActorComponent
{
	GENERATED_BODY()

public:
	UMoonTexturer();

	/** Uploads freshly computed scan pixels into the data texture and updates the material. */
	void ApplyScanData(const TArray<FLinearColor>& ScanPixels);

	/** Number of texels (= max scans) the data texture can hold. */
	int32 GetCapacity() const { return Capacity; }

protected:
	virtual void BeginPlay() override;

private:
	/** Creates the data texture + MID and binds them to the owning tileset. Safe to call more than once. */
	void InitializeRendering();

	/** The custom Cesium material (copy of MI_CesiumThreeOverlaysAndClipping) with the ScannedAreaData param. */
	UPROPERTY(EditAnywhere, Category = "Fog of War")
	UMaterialInstance* CesiumMaterial = nullptr;

	/** Max number of scans the texture can hold. */
	UPROPERTY(EditAnywhere, Category = "Fog of War")
	int32 Capacity = 256;

	/**
	 * Index of the material LAYER that contains ScannedAreaData / NumScans.
	 * Layer parameters are namespaced by layer index (background layer = 0, first added layer = 1, ...),
	 * so they must be set via FMaterialParameterInfo, not by plain name. Set this to match your stack.
	 */
	UPROPERTY(EditAnywhere, Category = "Fog of War")
	int32 FogLayerIndex = 0;

	/**
	 * Collection carrying NumScans. A scalar set on the FogMID does NOT reach Cesium's already-created
	 * per-tile MIDs (so old/streamed tiles keep a stale value). An MPC is a global uniform every tile
	 * shader reads live — assign your MPC here and read NumScans from it in the material.
	 */
	UPROPERTY(EditAnywhere, Category = "Fog of War")
	UMaterialParameterCollection* ScanParameterCollection = nullptr;

	UPROPERTY(Transient)
	UTexture2D* FogOfWarTexture = nullptr;

	UPROPERTY(Transient)
	UMaterialInstanceDynamic* FogMID = nullptr;

	bool bInitialized = false;
};
