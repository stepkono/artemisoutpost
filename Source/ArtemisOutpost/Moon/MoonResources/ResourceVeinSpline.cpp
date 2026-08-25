// Fill out your copyright notice in the Description page of Project Settings.


#include "ResourceVeinSpline.h"

#include "ResourceVeinSubsystem.h"
#include "EngineUtils.h"
#include "ArtemisOutpost/Cesium/GeoRefsManager.h"
#include "Net/UnrealNetwork.h"


UResourceVeinSpline::UResourceVeinSpline()
{
	// Geometry is rebuilt event-driven (OnConstruction / explicit calls); state changes
	// are event-driven (detection reports). Nothing to do per frame.
	PrimaryComponentTick.bCanEverTick = false;

	// Replicate MinedOut to clients (needs the owning actor to replicate too).
	SetIsReplicatedByDefault(true);
}

void UResourceVeinSpline::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UResourceVeinSpline, MinedOut);
}

void UResourceVeinSpline::BeginPlay()
{
	Super::BeginPlay();

	// Resolve the georeference manager (same lookup pattern as MoonDataManager).
	for (TActorIterator<AGeoRefsManager> It(GetWorld()); It; ++It)
	{
		GeoRefs = *It;
		break;
	}
	if (!GeoRefs)
	{
		UE_LOG(LogTemp, Error, TEXT("[ResourceVein] No GeoRefsManager found — cannot bake geodetic samples."));
	}

	BakeSamples();

	// Self-register so the subsystem can route reports/queries to us.
	if (UResourceVeinSubsystem* Subsys = GetWorld()->GetSubsystem<UResourceVeinSubsystem>())
	{
		Subsys->RegisterVein(this);
	}
}

void UResourceVeinSpline::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UResourceVeinSubsystem* Subsys = World->GetSubsystem<UResourceVeinSubsystem>())
		{
			Subsys->UnregisterVein(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

FVector UResourceVeinSpline::GeoToSurfaceWorld(const FVector& Geo) const
{
	// Drop the height -> project onto the reference sphere, so the comparison ignores depth.
	return GeoRefs ? GeoRefs->VRMoonCoordsToUECoords(FVector(Geo.X, Geo.Y, 0.f)) : FVector::ZeroVector;
}

void UResourceVeinSpline::ComputeSampleDistances(TArray<float>& OutDistances) const
{
	OutDistances.Reset();

	const float TotalLength = GetSplineLength();
	if (TotalLength <= 0.f)
	{
		return;
	}

	const float Step = FMath::Max(SampleSpacing, 10.f);
	for (float Dist = 0.f; Dist < TotalLength; Dist += Step)
	{
		OutDistances.Add(Dist);
	}

	if (OutDistances.Num() == 0 || (TotalLength - OutDistances.Last()) > KINDA_SMALL_NUMBER)
	{
		OutDistances.Add(TotalLength);
	}
}

void UResourceVeinSpline::BakeSamples()
{
	Samples.Reset();
	SamplesSurfaceWorld.Reset();
	Discovered.Reset();
	MinedOut.Reset();
	MinedLocal.Reset();
	LocallyReportedDiscovered.Reset();

	if (!GeoRefs)
	{
		return;
	}

	TArray<float> Distances;
	ComputeSampleDistances(Distances);
	if (Distances.Num() == 0)
	{
		return;
	}

	for (const float Dist : Distances)
	{
		const FVector WorldPos = GetLocationAtDistanceAlongSpline(Dist, ESplineCoordinateSpace::World);
		Samples.Add(GeoRefs->UECoordsToVRMoonCoords(WorldPos));
	}

	SamplesSurfaceWorld.Reserve(Samples.Num());
	for (const FVector& Geo : Samples)
	{
		SamplesSurfaceWorld.Add(GeoToSurfaceWorld(Geo));
	}

	const int32 N = Samples.Num();
	Discovered.Init(0, N);
	MinedOut.Init(0, N);
	MinedLocal.Init(0.f, N);
	LocallyReportedDiscovered.Init(0, N);

	UE_LOG(LogTemp, Log, TEXT("[ResourceVein] Baked %d samples (length %.0f UU, step %.0f)."),
		N, GetSplineLength(), FMath::Max(SampleSpacing, 10.f));

	// All segments visible at full scale until mined.
	RefreshVeinVisual();
}

void UResourceVeinSpline::DetectNewDiscovered(const FVector& HitWorld, float RadiusWorld, TArray<int32>& OutNewIndices)
{
	OutNewIndices.Reset();
	if (Samples.Num() == 0 || !GeoRefs)
	{
		return;
	}

	const FVector HitSurface = GeoToSurfaceWorld(GeoRefs->UECoordsToVRMoonCoords(HitWorld));
	const float RadiusSq = RadiusWorld * RadiusWorld;

	for (int32 i = 0; i < SamplesSurfaceWorld.Num(); ++i)
	{
		if (LocallyReportedDiscovered[i])
		{
			continue;  // already reported by THIS client -> event-gate
		}
		if (FVector::DistSquared(HitSurface, SamplesSurfaceWorld[i]) <= RadiusSq)
		{
			LocallyReportedDiscovered[i] = 1;
			OutNewIndices.Add(i);
		}
	}
}

void UResourceVeinSpline::PredictMining(const FVector& HitWorld, float RadiusWorld, float DeltaSeconds,
                                        float RatePerSecond, TArray<int32>& OutCompletedIndices)
{
	OutCompletedIndices.Reset();
	if (Samples.Num() == 0 || !GeoRefs)
	{
		return;
	}

	const FVector HitSurface = GeoToSurfaceWorld(GeoRefs->UECoordsToVRMoonCoords(HitWorld));
	const float RadiusSq = RadiusWorld * RadiusWorld;
	const float Delta = RatePerSecond * DeltaSeconds;

	for (int32 i = 0; i < SamplesSurfaceWorld.Num(); ++i)
	{
		// Skip what is already locally done or authoritatively mined out.
		if (MinedLocal[i] >= 1.f || MinedOut[i])
		{
			continue;
		}
		if (FVector::DistSquared(HitSurface, SamplesSurfaceWorld[i]) <= RadiusSq)
		{
			MinedLocal[i] = FMath::Clamp(MinedLocal[i] + Delta, 0.f, 1.f);
			UpdateSegmentVisual(i);   // smooth local thinning

			if (MinedLocal[i] >= 1.f)
			{
				OutCompletedIndices.Add(i);   // report this completion once
			}
		}
	}
}

void UResourceVeinSpline::ServerMarkDiscovered(const TArray<int32>& Indices)
{
	TArray<FVector> NewlyGeo;
	for (const int32 i : Indices)
	{
		if (Discovered.IsValidIndex(i) && !Discovered[i])
		{
			Discovered[i] = 1;                 // idempotent: only newly-set count
			NewlyGeo.Add(Samples[i]);          // coords for the bridge
		}
	}

	if (NewlyGeo.Num() > 0)
	{
		OnSamplesDiscovered.Broadcast(NewlyGeo);
	}
}

void UResourceVeinSpline::ServerMarkMinedOut(const TArray<int32>& Indices)
{
	TArray<FVector> NewlyGeo;
	for (const int32 i : Indices)
	{
		if (MinedOut.IsValidIndex(i) && !MinedOut[i])
		{
			MinedOut[i] = 1;                   // replicated -> OnRep on clients
			NewlyGeo.Add(Samples[i]);
			UpdateSegmentVisual(i);            // host sees it immediately (no OnRep on authority)
		}
	}

	if (NewlyGeo.Num() > 0)
	{
		OnSamplesMined.Broadcast(NewlyGeo);
	}
}

ERessourceType UResourceVeinSpline::QueryResourceAt(const FVector& GeoPos, float RadiusWorld) const
{
	if (SamplesSurfaceWorld.Num() == 0)
	{
		return ERessourceType::NoneRessource;
	}

	const FVector QuerySurface = GeoToSurfaceWorld(GeoPos);
	const float RadiusSq = RadiusWorld * RadiusWorld;

	for (int32 i = 0; i < SamplesSurfaceWorld.Num(); ++i)
	{
		if (Discovered[i] && FVector::DistSquared(QuerySurface, SamplesSurfaceWorld[i]) <= RadiusSq)
		{
			return ResourceType;
		}
	}

	return ERessourceType::NoneRessource;
}

void UResourceVeinSpline::UpdateSegmentVisual(int32 SegmentIndex)
{
	if (!SegmentMeshes.IsValidIndex(SegmentIndex) || !IsValid(SegmentMeshes[SegmentIndex]))
	{
		return;
	}

	USplineMeshComponent* Seg = SegmentMeshes[SegmentIndex];

	// Visibility is driven ONLY by mining: authoritative MinedOut (all clients) OR the local
	// prediction reaching full (the mining client, before the server confirms). Discovery does
	// not affect the VR mesh.
	const bool bMinedOut  = MinedOut.IsValidIndex(SegmentIndex) && MinedOut[SegmentIndex] != 0;
	const float LocalMine = MinedLocal.IsValidIndex(SegmentIndex) ? MinedLocal[SegmentIndex] : 0.f;

	if (bMinedOut || LocalMine >= 0.999f)
	{
		Seg->SetVisibility(false);
		return;
	}

	Seg->SetVisibility(true);

	// Thin the cross-section by the local mining prediction (0 on non-mining clients -> full).
	const FVector2D ScaledCrossSection = VeinScale * FMath::Max(1.f - LocalMine, 0.f);
	Seg->SetStartScale(ScaledCrossSection, /*bUpdateMesh=*/false);
	Seg->SetEndScale(ScaledCrossSection,   /*bUpdateMesh=*/true);
}

void UResourceVeinSpline::RefreshVeinVisual()
{
	const int32 Count = SegmentMeshes.Num();
	for (int32 i = 0; i < Count; ++i)
	{
		UpdateSegmentVisual(i);
	}
}

void UResourceVeinSpline::OnRep_MinedOut()
{
	// Server confirmed mined-out samples -> re-drive the visual on this client. Index guards
	// in UpdateSegmentVisual cover the case where this arrives before the mesh is built.
	RefreshVeinVisual();
}

void UResourceVeinSpline::BuildMesh()
{
	// Clear the previously generated segments. BuildMesh runs on every spline edit
	// (via the actor's OnConstruction), so without this the segments would pile up.
	for (USplineMeshComponent* Seg : SegmentMeshes)
	{
		if (IsValid(Seg))
		{
			Seg->DestroyComponent();
		}
	}
	SegmentMeshes.Reset();

	// Nothing assigned yet -> render nothing (valid intermediate state while authoring).
	if (!VeinMesh)
	{
		return;
	}

	// One segment per SAMPLE interval (same arc-length grid as BakeSamples), so each
	// segment maps 1:1 to a sample and can be mined individually.
	TArray<float> Distances;
	ComputeSampleDistances(Distances);
	if (Distances.Num() < 2)
	{
		return;
	}

	for (int32 i = 0; i < Distances.Num() - 1; ++i)
	{
		const float DistA = Distances[i];
		const float DistB = Distances[i + 1];
		const float SegLength = DistB - DistA;

		const FVector StartPos = GetLocationAtDistanceAlongSpline(DistA, ESplineCoordinateSpace::Local);
		const FVector EndPos   = GetLocationAtDistanceAlongSpline(DistB, ESplineCoordinateSpace::Local);
		const FVector StartTangent = GetDirectionAtDistanceAlongSpline(DistA, ESplineCoordinateSpace::Local) * SegLength;
		const FVector EndTangent   = GetDirectionAtDistanceAlongSpline(DistB, ESplineCoordinateSpace::Local) * SegLength;

		USplineMeshComponent* Seg = NewObject<USplineMeshComponent>(GetOwner());
		Seg->SetMobility(EComponentMobility::Movable);
		Seg->SetupAttachment(this);
		Seg->RegisterComponent();

		Seg->SetStaticMesh(VeinMesh);
		if (VeinMaterial)
		{
			Seg->SetMaterial(0, VeinMaterial);
		}

		Seg->SetForwardAxis(ForwardAxis, /*bUpdateMesh=*/false);
		Seg->SetStartAndEnd(StartPos, StartTangent, EndPos, EndTangent, /*bUpdateMesh=*/false);
		Seg->SetStartScale(VeinScale, /*bUpdateMesh=*/false);
		Seg->SetEndScale(VeinScale,   /*bUpdateMesh=*/true);

		Seg->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		SegmentMeshes.Add(Seg);
	}
}
