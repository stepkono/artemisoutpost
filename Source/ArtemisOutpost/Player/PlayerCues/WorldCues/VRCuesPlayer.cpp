// Fill out your copyright notice in the Description page of Project Settings.

#include "VRCuesPlayer.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"

AVRCuesPlayer::AVRCuesPlayer()
{
	Head = CreateDefaultSubobject<USceneComponent>(TEXT("Head"));
	Head->SetupAttachment(Root);

	LeftHand = CreateDefaultSubobject<USceneComponent>(TEXT("LeftHand"));
	LeftHand->SetupAttachment(Root);

	RightHand = CreateDefaultSubobject<USceneComponent>(TEXT("RightHand"));
	RightHand->SetupAttachment(Root);

	// Placed in world space by the Blueprint every tick, so it hangs off Root and not off Head.
	PlayerCueCylinder = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlayerCueCylinder"));
	PlayerCueCylinder->SetupAttachment(Root);
	PlayerCueCylinder->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlayerCueCylinder->SetCastShadow(false);
	PlayerCueCylinder->SetVisibility(false);
}

float AVRCuesPlayer::DistanceToNetWall(const FVector& Point) const
{
	if (!PlayerCueCylinder)
	{
		return TNumericLimits<float>::Max();
	}

	const FTransform& CylinderTransform = PlayerCueCylinder->GetComponentTransform();
	const FVector Axis       = CylinderTransform.GetUnitAxis(EAxis::Z);
	const FVector Scale      = CylinderTransform.GetScale3D().GetAbs();
	const double  Radius     = NetMeshRadius * Scale.X;
	const double  HalfLength = 0.5 * NetMeshLength * Scale.Z;

	// Relative to the center: height along the axis and distance from the axis.
	const FVector Relative = Point - CylinderTransform.GetLocation();
	const double  Height   = FVector::DotProduct(Relative, Axis);
	const double  Radial   = (Relative - Height * Axis).Size();

	const double RadialGap = FMath::Abs(Radial - Radius);
	const double HeightGap = FMath::Max(0.0, FMath::Abs(Height) - HalfLength);
	return static_cast<float>(FMath::Sqrt(RadialGap * RadialGap + HeightGap * HeightGap));
}

bool AVRCuesPlayer::UpdateNetCylinderVisibility()
{
	if (!PlayerCueCylinder)
	{
		return false;
	}

	const float HeadDistance  = DistanceToNetWall(ViewerTrackers.Head.GetLocation());
	const float LeftDistance  = ViewerTrackers.bLeftTracked  ? DistanceToNetWall(ViewerTrackers.Left.GetLocation())  : TNumericLimits<float>::Max();
	const float RightDistance = ViewerTrackers.bRightTracked ? DistanceToNetWall(ViewerTrackers.Right.GetLocation()) : TNumericLimits<float>::Max();
	const float MinDistance   = FMath::Min3(HeadDistance, LeftDistance, RightDistance);

	// The material is already at zero glow beyond ProximityThreshold, so switching here never pops.
	const float Limit = ProximityThreshold + GateMargin;
	const bool bVisible = MinDistance < Limit;
	PlayerCueCylinder->SetVisibility(bVisible);

	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	if (bLogNetGate && Now - LastNetGateLogTime >= 1.0)
	{
		LastNetGateLogTime = Now;
		const FTransform& CylinderTransform = PlayerCueCylinder->GetComponentTransform();
		UE_LOG(LogTemp, Log, TEXT("[WorldCues] NetGate %s | min=%.0f (head=%.0f L=%.0f R=%.0f) limit=%.0f visible=%d | compVisible=%d hiddenInGame=%d actorHidden=%d | mesh=%s mat=%s | cyl=%s scale=%s | viewerHead=%s | pose.Head=%s (anchors-frame fractions, expect |x| < ~3) headComp=%s"),
			*GetName(), MinDistance, HeadDistance,
			ViewerTrackers.bLeftTracked ? LeftDistance : -1.0f, ViewerTrackers.bRightTracked ? RightDistance : -1.0f,
			Limit, bVisible ? 1 : 0,
			PlayerCueCylinder->IsVisible() ? 1 : 0, PlayerCueCylinder->bHiddenInGame ? 1 : 0, IsHidden() ? 1 : 0,
			*GetNameSafe(PlayerCueCylinder->GetStaticMesh()), *GetNameSafe(PlayerCueCylinder->GetMaterial(0)),
			*CylinderTransform.GetLocation().ToString(), *CylinderTransform.GetScale3D().ToString(),
			*ViewerTrackers.Head.GetLocation().ToString(),
			*Pose.Head.GetLocation().ToString(), Head ? *Head->GetComponentLocation().ToString() : TEXT("none"));
	}
	return bVisible;
}
