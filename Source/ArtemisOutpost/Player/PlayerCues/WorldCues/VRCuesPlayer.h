// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VRWorldCue.h"
#include "VRCuesPlayer.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;

/**
 * One remote player's head and controllers, as seen by the local player in VR. A vessel: the presenter writes Pose
 * and ViewerTrackers every tick, the Blueprint child moves Head / LeftHand / RightHand from Pose (anchors frame ->
 * world via UAnchorsManagerSubsystem::GetWorldFromAnchorsFrame, then interpolation) and fits PlayerCueCylinder
 * around them, with the proximity glow driven by ViewerTrackers.
 */
UCLASS(Blueprintable)
class ARTEMISOUTPOST_API AVRCuesPlayer : public AVRWorldCue
{
	GENERATED_BODY()

public:
	AVRCuesPlayer();

	// Read-only access for diagnostics.
	const USceneComponent* GetHeadComponent() const      { return Head; }
	const USceneComponent* GetLeftHandComponent() const  { return LeftHand; }
	const USceneComponent* GetRightHandComponent() const { return RightHand; }

	// ---- Net cylinder render gate ----

	// Distance (UE units) from Point to the cylinder WALL as currently placed (PlayerCueCylinder's world transform:
	// axis = its Z, radius = NetMeshRadius * scale X, length = NetMeshLength * scale Z). The caps count as open: above
	// or below the cylinder the distance is measured to the rim.
	UFUNCTION(BlueprintPure, Category = "World Cues|VR|Net Cylinder")
	float DistanceToNetWall(const FVector& Point) const;

	// Shows PlayerCueCylinder only while one of the viewer's trackers (head always, controllers only while tracked) is
	// closer to its wall than ProximityThreshold + GateMargin. Call it after the cylinder was placed this tick.
	// Returns the new visibility.
	UFUNCTION(BlueprintCallable, Category = "World Cues|VR|Net Cylinder")
	bool UpdateNetCylinderVisibility();

	// Size of SM_PlayerCueCylinder at scale 1, pivot in its center. Keep in sync with the mesh and the net material.
	static constexpr float NetMeshRadius = 50.0f;
	static constexpr float NetMeshLength = 100.0f;
	
	UPROPERTY(BlueprintReadWrite, Category = "World Cues|VR")
	bool bHasNetRadius = false;

	// ---- Net cylinder runtime state (Blueprint logic) ----

	// Created in BeginPlay from PlayerCueCylinder's material.
	UPROPERTY(Transient, BlueprintReadWrite, Category = "World Cues|VR|Net Cylinder")
	TObjectPtr<UMaterialInstanceDynamic> NetMID;

	// Smoothed radius the cylinder is drawn with.
	UPROPERTY(Transient, BlueprintReadWrite, Category = "World Cues|VR|Net Cylinder")
	float CurrentRadius = 35.0f;

	// ---- Net cylinder tunables, edited in the Blueprint class defaults. All lengths in UE units (cm). ----
	// Axis = moon center -> head, top edge at head + HeadClearance, radius = the larger horizontal head->controller
	// distance (untracked controllers ignored).

	// Length along the axis, extending down from the top edge.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Cues|VR|Net Cylinder", meta = (ClampMin = "1.0"))
	float CylinderLength = 150.0f;

	// How far the top edge sits above the head.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Cues|VR|Net Cylinder")
	float HeadClearance = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Cues|VR|Net Cylinder", meta = (ClampMin = "1.0"))
	float MinRadius = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Cues|VR|Net Cylinder", meta = (ClampMin = "1.0"))
	float MaxRadius = 90.0f;

	// Used while neither controller is tracked and no radius was measured yet.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Cues|VR|Net Cylinder", meta = (ClampMin = "1.0"))
	float DefaultRadius = 35.0f;

	// FInterpTo speed of the radius. 0 = no smoothing.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Cues|VR|Net Cylinder", meta = (ClampMin = "0.0"))
	float RadiusInterpSpeed = 6.0f;

	// The lines start to glow when one of the local trackers is closer than this.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Cues|VR|Proximity", meta = (ClampMin = "0.0"))
	float ProximityThreshold = 40.0f;

	// Full glow at this distance or closer. Must stay below ProximityThreshold.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Cues|VR|Proximity", meta = (ClampMin = "0.0"))
	float ProximityFullGlow = 10.0f;

	// The cylinder mesh is only rendered while a local tracker is within ProximityThreshold + GateMargin of its wall.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Cues|VR|Proximity", meta = (ClampMin = "0.0"))
	float GateMargin = 10.0f;

	// Once per second while the gate runs: distances, limit, result, where the cylinder and the viewer head are.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Cues|VR|Proximity")
	bool bLogNetGate = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Cues|VR|Net Look")
	FLinearColor LineColor = FLinearColor(0.0f, 0.8f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Cues|VR|Net Look", meta = (ClampMin = "0.0"))
	float GlowIntensity = 1.5f;

protected:
	// Targets for the Blueprint movement logic. Add the meshes as children of these in the Blueprint.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Cues|VR")
	TObjectPtr<USceneComponent> Head;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Cues|VR")
	TObjectPtr<USceneComponent> LeftHand;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Cues|VR")
	TObjectPtr<USceneComponent> RightHand;

	// The net cylinder. Mesh (SM_PlayerCueCylinder, radius 50, height 100, pivot centered) and material are set in
	// the Blueprint defaults. Starts hidden, the proximity gate in the Blueprint switches its visibility.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Cues|VR")
	TObjectPtr<UStaticMeshComponent> PlayerCueCylinder;

private:
	double LastNetGateLogTime = -1000.0;
};
