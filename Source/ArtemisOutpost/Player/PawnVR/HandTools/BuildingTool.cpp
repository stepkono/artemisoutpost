// Fill out your copyright notice in the Description page of Project Settings.

#include "BuildingTool.h"

#include "ArtemisOutpost/MiniGames/General/GameInstance/MinigameActor.h"
#include "ArtemisOutpost/Moon/Cesium/GeoRefsManager.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "EngineUtils.h"

#define LOCTEXT_NAMESPACE "BuildingTool"

ABuildingTool::ABuildingTool()
{
	// Replicated so the Server RPC routes from this client-owned tool. Set Owner on the Child Actor
	// Component makes the pawn its Owner, which gives it an owning connection.
	bReplicates = true;
}

void ABuildingTool::BeginPlay()
{
	Super::BeginPlay();

	GeoRefsManager = Cast<AGeoRefsManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AGeoRefsManager::StaticClass()));
	if (!GeoRefsManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildingTool] No AGeoRefsManager found; surface arc falls back to world-up."));
	}
}

FVector ABuildingTool::GetSurfaceUp(FVector WorldPos) const
{
	if (!GeoRefsManager)
	{
		return FVector::UpVector;
	}

	// Geodetic up = direction from the point to the same lon/lat one metre higher (Height = LLH.Z).
	const FVector LLH = GeoRefsManager->UECoordsToVRMoonCoords(WorldPos);
	const FVector HigherWorld = GeoRefsManager->VRMoonCoordsToUECoords(LLH + FVector(0.0, 0.0, 100.0));

	const FVector Up = (HigherWorld - WorldPos).GetSafeNormal();
	return Up.IsNearlyZero() ? FVector::UpVector : Up;
}

void ABuildingTool::BeginPlacement(EOutpostBuildingType BuildingType)
{
	UE_LOG(LogTemp, Warning, TEXT("[BuildingTool] BeginPlacement: Type=%s (was %s)"),
		*UEnum::GetValueAsString(BuildingType), *UEnum::GetValueAsString(CurrentBuildingType));

	CurrentBuildingType = BuildingType;
	if (!bToolActive)
	{
		ActivateTool();
	}
	
	TogglePlacementVisual(true);  
	bPlacementBegan = true; 
}

bool ABuildingTool::CanPlaceBuildingAt(FVector Location, FVector SurfaceNormal, FText& OutReason) const
{
	OutReason = FText::GetEmpty();

	// Reference "up" = geodetic up at the build spot. Do NOT use the pawn's actor up: the VR capsule
	// stays WORLD-up aligned (only the view tilts to the surface), so on the moon its up is world-up
	// and every slope check would wrongly fail.
	const FVector ReferenceUp = GetSurfaceUp(Location);

	const float CosAngle = FVector::DotProduct(SurfaceNormal.GetSafeNormal(), ReferenceUp.GetSafeNormal());
	const float SlopeDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(CosAngle, -1.0f, 1.0f)));

	UE_LOG(LogTemp, Warning, TEXT("[BuildingTool] CanPlaceBuildingAt: Loc=%s Normal=%s RefUp=%s Slope=%.1f (max %.1f) GeoRefs=%s"),
		*Location.ToCompactString(), *SurfaceNormal.GetSafeNormal().ToCompactString(),
		*ReferenceUp.ToCompactString(), SlopeDeg, MaxPlacementSlopeDegrees,
		GeoRefsManager ? TEXT("OK") : TEXT("NULL"));

	if (SlopeDeg > MaxPlacementSlopeDegrees)
	{
		OutReason = LOCTEXT("TooSteep", "Untergrund zu steil");
		UE_LOG(LogTemp, Warning, TEXT("[BuildingTool]   REJECT: too steep (%.1f > %.1f)"), SlopeDeg, MaxPlacementSlopeDegrees);
		return false;
	}

	// Proximity: reject if too close to an existing building. Iterating AMinigameActor is RHI-safe
	// (no collision query) — all buildings derive from AMinigameActor.
	if (const UWorld* World = GetWorld())
	{
		for (TActorIterator<AMinigameActor> It(World); It; ++It)
		{
			const AMinigameActor* Existing = *It;
			if (!Existing)
			{
				continue;
			}
			const float Dist = FVector::Dist(Existing->GetActorLocation(), Location);
			if (Dist < MinBuildingSpacing)
			{
				OutReason = LOCTEXT("TooClose", "Zu nah an einem Gebäude");
				UE_LOG(LogTemp, Warning, TEXT("[BuildingTool]   REJECT: too close to %s (%.0f < %.0f cm)"),
					*GetNameSafe(Existing), Dist, MinBuildingSpacing);
				return false;
			}
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[BuildingTool]   OK: placement allowed"));
	return true;
}

bool ABuildingTool::TryBuildAtPlacement(FVector PlacementLocation)
{
	// Only build when a placement is actually in progress. This rejects (a) confirms that fire before
	// BeginPlacement set the type/started the arc, and (b) the extra fires of a single trigger press
	// (the first success sets bPlacementBegan=false, so the rest are ignored).
	if (!bPlacementBegan)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildingTool] TryBuildAtPlacement ignored: no active placement."));
		return false;
	}

	// Client-side gate for immediate feedback; the server re-validates authoritatively anyway.
	FText Reason;
	const FVector SurfaceNormal = GetSurfaceUp(PlacementLocation);

	UE_LOG(LogTemp, Warning, TEXT("[BuildingTool] TryBuildAtPlacement: Loc=%s Type=%s"),
		*PlacementLocation.ToCompactString(), *UEnum::GetValueAsString(CurrentBuildingType));

	if (!CanPlaceBuildingAt(PlacementLocation, SurfaceNormal, Reason))
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildingTool] TryBuildAtPlacement -> FALSE: %s"), *Reason.ToString());
		return false;
	}

	const FQuat TargetQuat = FQuat::FindBetweenNormals(FVector::UpVector, SurfaceNormal);
	FTransform PlacementTransform;
	PlacementTransform.SetLocation(PlacementLocation);
	PlacementTransform.SetRotation(TargetQuat);

	UE_LOG(LogTemp, Warning, TEXT("[BuildingTool] TryBuildAtPlacement -> TRUE, calling ServerPlaceBuilding"));
	ServerPlaceBuilding(CurrentBuildingType, PlacementTransform);
	
	TogglePlacementVisual(false); 
	bPlacementBegan = false; 
	
	return true;
}

void ABuildingTool::ExecuteAction()
{
	// Trigger DOWN while this is the active tool -> build at the arc's current landing point (written
	// by the BP arc into ProposedBuildLocation each tick).
	TryBuildAtPlacement(ProposedBuildLocation);
}

//TODO: pay attention if this actually does work, since the SetOwner is set on the BP. If this RPC never runs on server, the ownership problem is the first suspsect
void ABuildingTool::ServerPlaceBuilding_Implementation(EOutpostBuildingType BuildingType, FTransform PlacementTransform)
{
	// Authority re-check. The client already validated for feedback, but the server must never trust
	// it. The transform's Z axis is the surface normal (the client aligned it to the ground).
	FText Reason;
	if (!CanPlaceBuildingAt(PlacementTransform.GetLocation(), PlacementTransform.GetUnitAxis(EAxis::Z), Reason))
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildingTool] ServerPlaceBuilding rejected (%s): %s"),
			*UEnum::GetValueAsString(BuildingType), *Reason.ToString());
		return;
	}

	const TSubclassOf<AMinigameActor>* ClassPtr = BuildingClasses.Find(BuildingType);
	if (!ClassPtr || !*ClassPtr)
	{
		UE_LOG(LogTemp, Error, TEXT("[BuildingTool] ServerPlaceBuilding: no class mapped for %s. Fill BuildingClasses."),
			*UEnum::GetValueAsString(BuildingType));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AMinigameActor* NewBuilding = World->SpawnActor<AMinigameActor>(*ClassPtr, PlacementTransform, SpawnParams);
	UE_LOG(LogTemp, Log, TEXT("[BuildingTool] Placed building %s -> %s at %s"),
		*UEnum::GetValueAsString(BuildingType), *GetNameSafe(NewBuilding), *PlacementTransform.GetLocation().ToString());
}

bool ABuildingTool::PredictArcOnSurface(FVector StartPos, FVector LaunchVelocity,
	const TArray<TEnumAsByte<EObjectTypeQuery>>& ObjectTypes,
	const TArray<AActor*>& ActorsToIgnore,
	TArray<FVector>& OutPathPositions, FHitResult& OutHit,
	bool bInertia, float GravityMagnitude, float ProjectileRadius,
	float SimFrequency, float MaxSimTime, float MaxLaunchAngleDeg, bool bTraceComplex)
{
	OutPathPositions.Reset();
	OutHit = FHitResult();

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const float DeltaSeconds = World->GetDeltaSeconds();
	const FVector Up = GetSurfaceUp(StartPos);

	// --- Elevation clamp: never let the launch point higher than MaxLaunchAngleDeg above the local
	// horizon, so the arc always comes back down within MaxSimTime (fixes "aim too high -> never
	// reaches the ground"). Aiming higher than the cap just aims at the max-range arc. ---
	FVector EffectiveVel = LaunchVelocity;
	const float Speed = EffectiveVel.Size();
	if (Speed > KINDA_SMALL_NUMBER)
	{
		const FVector Dir = EffectiveVel / Speed;
		const float SinElev = FVector::DotProduct(Dir, Up);
		const float MaxSin = FMath::Sin(FMath::DegreesToRadians(MaxLaunchAngleDeg));
		if (SinElev > MaxSin)
		{
			FVector Horizontal = (Dir - Up * SinElev).GetSafeNormal();
			if (Horizontal.IsNearlyZero())
			{
				// Pointing straight along Up — pick any tangent so we still get a valid arc.
				Horizontal = FVector::CrossProduct(Up, FVector::ForwardVector).GetSafeNormal();
				if (Horizontal.IsNearlyZero())
				{
					Horizontal = FVector::CrossProduct(Up, FVector::RightVector).GetSafeNormal();
				}
			}
			const float CosMax = FMath::Cos(FMath::DegreesToRadians(MaxLaunchAngleDeg));
			EffectiveVel = (Horizontal * CosMax + Up * MaxSin) * Speed;
		}
	}

	// --- Ballistic simulation. Gravity is recomputed each step toward the LOCAL surface, so the arc
	// follows the moon's curvature instead of a fixed-direction parabola — that's what lets shots
	// near/above the horizon still curve back down to the ground. ---
	const float GravMag = FMath::Abs(GravityMagnitude);
	const float SubstepTime = (SimFrequency > KINDA_SMALL_NUMBER) ? (1.0f / SimFrequency) : (1.0f / 15.0f);
	const int32 MaxSteps = FMath::Max(1, FMath::CeilToInt(MaxSimTime / SubstepTime));

	FCollisionObjectQueryParams ObjectParams;
	for (const TEnumAsByte<EObjectTypeQuery>& OT : ObjectTypes)
	{
		ObjectParams.AddObjectTypesToQuery(UEngineTypes::ConvertToCollisionChannel(OT));
	}

	FCollisionQueryParams QueryParams(TEXT("PredictArcOnSurface"), bTraceComplex);
	QueryParams.AddIgnoredActor(this);
	if (const AActor* OwnerActor = GetOwner())
	{
		QueryParams.AddIgnoredActor(OwnerActor);
	}
	QueryParams.AddIgnoredActors(ActorsToIgnore);

	TArray<FVector> RawPath;
	RawPath.Add(StartPos);
	FVector CurrentPos = StartPos;
	FVector CurrentVel = EffectiveVel;
	bool bHit = false;

	for (int32 Step = 0; Step < MaxSteps; ++Step)
	{
		// Semi-implicit Euler, with gravity toward the surface at the CURRENT position (curvature-aware).
		const FVector StepGravity = GetSurfaceUp(CurrentPos) * (-GravMag);
		CurrentVel += StepGravity * SubstepTime;
		const FVector NextPos = CurrentPos + CurrentVel * SubstepTime;

		FHitResult Hit;
		const bool bBlocking = (ProjectileRadius > KINDA_SMALL_NUMBER)
			? World->SweepSingleByObjectType(Hit, CurrentPos, NextPos, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(ProjectileRadius), QueryParams)
			: World->LineTraceSingleByObjectType(Hit, CurrentPos, NextPos, ObjectParams, QueryParams);

		if (bBlocking)
		{
			OutHit = Hit;
			RawPath.Add(Hit.ImpactPoint);
			bHit = true;
			break;
		}

		RawPath.Add(NextPos);
		CurrentPos = NextPos;
	}

	UE_LOG(LogTemp, Warning, TEXT("[BuildingTool] PredictArc: bHit=%d Impact=%s Pts=%d MaxSteps=%d ObjTypes=%d Radius=%.0f"),
		bHit, *OutHit.ImpactPoint.ToCompactString(), RawPath.Num(), MaxSteps, ObjectTypes.Num(), ProjectileRadius);

	// --- Inertia: resample to a fixed count and lag each point toward the fresh arc. Far points move
	// more than near ones, so the curve trails / bends when you swing the controller. ---
	if (bInertia && RawPath.Num() >= 2 && InertiaPointCount >= 2)
	{
		TArray<FVector> Resampled;
		Resampled.SetNumUninitialized(InertiaPointCount);
		const int32 LastRaw = RawPath.Num() - 1;
		for (int32 k = 0; k < InertiaPointCount; ++k)
		{
			const float Frac = (float)k / (float)(InertiaPointCount - 1);
			const float ScanPos = Frac * LastRaw;
			const int32 I0 = FMath::FloorToInt(ScanPos);
			const int32 I1 = FMath::Min(I0 + 1, LastRaw);
			Resampled[k] = FMath::Lerp(RawPath[I0], RawPath[I1], ScanPos - I0);
		}

		if (SmoothedPath.Num() != InertiaPointCount)
		{
			SmoothedPath = Resampled; // snap on first frame / when the count changes
		}
		else
		{
			// Pin the origin exactly to the muzzle so the beam always starts cleanly at the hand
			// (no lag at point 0 -> no hard kink right after the start).
			SmoothedPath[0] = Resampled[0];

			for (int32 k = 1; k < InertiaPointCount; ++k)
			{
				// Catch-up speed ramps fast (start / near the hand) -> slow (end / tip). Near points
				// snap to the new aim, far points lag behind -> the curve bends and trails when you
				// swing the controller. The falloff exponent eases the lag IN from the start so there
				// is no abrupt jump next to the pinned origin.
				const float Frac = (float)k / (float)(InertiaPointCount - 1);
				const float Eased = FMath::Pow(Frac, InertiaLagFalloff);
				const float PointSpeed = FMath::Lerp(InertiaSpeedStart, InertiaSpeedEnd, Eased);
				SmoothedPath[k] = FMath::VInterpTo(SmoothedPath[k], Resampled[k], DeltaSeconds, PointSpeed);
			}
		}
		OutPathPositions = SmoothedPath;
	}
	else
	{
		SmoothedPath.Reset(); // so re-enabling inertia snaps cleanly instead of jumping from stale data
		OutPathPositions = RawPath;
	}

	// bHit reflects the REAL landing (OutHit) — use it for placement even while the visual beam lags.
	return bHit;
}

#undef LOCTEXT_NAMESPACE
