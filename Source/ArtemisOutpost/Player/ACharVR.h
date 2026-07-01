// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Cesium3DTileset.h"
#include "CesiumGeoreference.h"
#include "ArtemisOutpost/Cesium/GeoRefsManager.h"
#include "GameFramework/Character.h"
#include "ACharVR.generated.h"

UCLASS()
class ARTEMISOUTPOST_API ACharVR : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ACharVR();
	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
	UFUNCTION(BlueprintCallable, Category = "VR Moon")
	ACesium3DTileset* GetVRTileset(); 
	
	UFUNCTION(BlueprintCallable, Category = "GeoRefsManager")
	void SetGeoRefsManager(AGeoRefsManager* InManager);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void NotifyControllerChanged() override;

	// Refuses Cesium streamed tiles as a movement base. They are created at runtime and are not
	// network-addressable, so CMC's base-relative replication can't resolve them across the wire
	// (spams FNetGUIDCache::SupportsObject "NOT Supported" and drops every ClientAdjustPosition
	// correction). Nulling the base makes the character replicate ABSOLUTE position instead, which
	// both peers resolve because the moon is at the same georeferenced UE location everywhere.
	virtual void SetBase(UPrimitiveComponent* NewBase, FName BoneName = NAME_None, bool bNotifyActor = true) override;

	UPROPERTY(BlueprintReadOnly, Category = "GeoRefsManager")
	AGeoRefsManager* GeoRefsManager;

	// ---- VR component tags (must match the tags set on the components in BP_VRChar) ----
	UPROPERTY(EditDefaultsOnly, Category = "VR")
	FName VROriginTag = TEXT("VR_Origin");

	UPROPERTY(EditDefaultsOnly, Category = "VR")
	FName VRCameraTag = TEXT("VR_Camera");

	// ---- Head / roof collision (keeps the HMD view from clipping through geometry above) ----
	// When true, if the player's real head rises into a ceiling the VR view is pushed back
	// down so the camera stays below the obstruction instead of seeing through it.
	UPROPERTY(EditDefaultsOnly, Category = "VR|Head Collision")
	bool bEnableHeadCollision = true;

	// Radius (cm) of the sphere swept up to the head; roughly the player's head half-width.
	UPROPERTY(EditDefaultsOnly, Category = "VR|Head Collision", meta = (ClampMin = "1.0"))
	float HeadCollisionRadius = 12.0f;

	// How quickly the push-down offset eases in/out. Higher = snappier, lower = smoother
	UPROPERTY(EditDefaultsOnly, Category = "VR|Head Collision", meta = (ClampMin = "0.0"))
	float HeadCollisionInterpSpeed = 15.0f;

private:
	// Resolves VROrigin/VRCamera (by tag) and applies the standing VR setup. Local + HMD only.
	void InitVRComponents();

	// Per-frame roof/ceiling avoidance for the HMD view. Local + HMD only.
	void UpdateHeadCollision(float DeltaTime);

	UPROPERTY()
	ACesium3DTileset* VRTileSet;

	// VR rig, resolved from BP_VRChar by tag in BeginPlay.
	// NOTE: deliberately NOT named VROrigin/VRCamera — those names belong to the components
	// authored in BP_VRChar, and matching them would collide with the generated BP properties.
	UPROPERTY(Transient)
	USceneComponent* CachedVROrigin = nullptr;

	UPROPERTY(Transient)
	class UCameraComponent* CachedVRCamera = nullptr;

	// Relative Z of VROrigin that places it at the bottom of the capsule (the player's feet),
	// i.e. the floor-level tracking origin. Computed once from the capsule half-height.
	float VROriginBaseZ = 0.0f;

	// Smoothed downward offset (cm) currently applied for head/roof collision.
	float CurrentHeadCollisionPush = 0.0f;
};
