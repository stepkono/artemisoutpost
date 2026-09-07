// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SplineMeshComponent.h"   // ESplineMeshAxis + USplineMeshComponent
#include "ArtemisOutpost/Miscellaneous/DataTypes.h"  // ERessourceType
#include "Components/SplineComponent.h"
#include "ResourceVeinSpline.generated.h"

class AGeoRefsManager;

// C++-only hooks, broadcast SERVER-SIDE only. Carry the GEODETIC positions (Lon, Lat, Height)
// of the samples that just changed — directly forwardable to the web. The subsystem subscribes
// and (later) the bridge forwards these deltas. (No networking of these delegates themselves.)
DECLARE_MULTICAST_DELEGATE_OneParam(FOnVeinSamplesChanged, const TArray<FVector>& /*ChangedGeoPositions*/);

/**
 * A multi-point spline that (1) auto-fills itself with a chain of USplineMeshComponents for
 * the visual, and (2) bakes itself into a dense, arc-length-uniform list of geodetic sample
 * points — the shared runtime form used for detection and (later) web sync.
 *
 * Authority model (variant A):
 *   - DETECTION runs CLIENT-LOCAL against this client's own raycast hit + its own baked
 *     samples (accurate, no interpolation). The client reports only monotonic per-sample
 *     FLIP EVENTS (never per-tick hit streams).
 *   - The SERVER owns the canonical state: Discovered (server-side, feeds query + bridge)
 *     and MinedOut (replicated, drives the mining visual everywhere).
 *   - Mining THINNING is a client-local prediction (MinedLocal); only the per-sample
 *     "mined out" flip is authoritative/replicated.
 *
 * The spline is authored UNDER the surface, so each sample's Height carries the depth;
 * detection projects to a reference sphere (Height 0) and is thus depth-independent.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ARTEMISOUTPOST_API UResourceVeinSpline : public USplineComponent
{
	GENERATED_BODY()

public:
	UResourceVeinSpline();

	// ---- Visual (editor-live, driven from the actor's OnConstruction) ----

	// Rebuilds the segment-mesh chain from the current spline points. Rebuild-safe.
	UFUNCTION(BlueprintCallable, Category = "Vein")
	void BuildMesh();

	// ---- Data (runtime) ----

	// Resamples the spline into arc-length-uniform geodetic points and resets state.
	void BakeSamples();

	// CLIENT: samples whose surface footprint is within RadiusWorld of the hit and that this
	// client has not yet reported. Marks them locally reported and returns their indices —
	// so the caller RPCs each flip exactly once (event-gated, never per tick).
	void DetectNewDiscovered(const FVector& HitWorld, float RadiusWorld, TArray<int32>& OutNewIndices);

	// CLIENT: advances the local mining prediction (MinedLocal) on in-range samples and drives
	// their thinning visual. Returns the indices that just reached fully-mined this call, so the
	// caller RPCs each completion once.
	void PredictMining(const FVector& HitWorld, float RadiusWorld, float DeltaSeconds,
	                   float RatePerSecond, TArray<int32>& OutCompletedIndices);

	// SERVER: mark the reported indices discovered (idempotent). Broadcasts the newly-set
	// coords via OnSamplesDiscovered (for the bridge). Discovered is not replicated to clients.
	void ServerMarkDiscovered(const TArray<int32>& Indices);

	// SERVER: mark the reported indices mined out (idempotent, replicated). Broadcasts the
	// newly-set coords via OnSamplesMined and updates the host visual; clients react via OnRep.
	void ServerMarkMinedOut(const TArray<int32>& Indices);

	// Read-side query (SERVER): this vein's resource type if a DISCOVERED sample lies within
	// RadiusWorld of GeoPos, otherwise NoneRessource.
	ERessourceType QueryResourceAt(const FVector& GeoPos, float RadiusWorld) const;

	const TArray<FVector>& GetSamples() const { return Samples; }
	ERessourceType GetResourceType() const { return ResourceType; }

	// Hooks (server-side broadcast) — see delegate note above.
	FOnVeinSamplesChanged OnSamplesDiscovered;
	FOnVeinSamplesChanged OnSamplesMined;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ---- Visual params ----

	UPROPERTY(EditAnywhere, Category = "Vein")
	UStaticMesh* VeinMesh = nullptr;

	UPROPERTY(EditAnywhere, Category = "Vein")
	UMaterialInterface* VeinMaterial = nullptr;

	UPROPERTY(EditAnywhere, Category = "Vein")
	FVector2D VeinScale = FVector2D(1.f, 1.f);

	UPROPERTY(EditAnywhere, Category = "Vein")
	TEnumAsByte<ESplineMeshAxis::Type> ForwardAxis = ESplineMeshAxis::X;

	// ---- Data params ----

	UPROPERTY(EditAnywhere, Category = "Vein")
	TEnumAsByte<ERessourceType> ResourceType = ERessourceType::REGOLITH;

	UPROPERTY(EditAnywhere, Category = "Vein", meta = (ClampMin = "10.0"))
	float SampleSpacing = 100.f;

private:
	FVector GeoToSurfaceWorld(const FVector& Geo) const;
	void ComputeSampleDistances(TArray<float>& OutDistances) const;
	void RefreshVeinVisual();

	// Drives one segment's visual: hidden once mined out (authoritative) OR fully predicted-
	// mined (local), otherwise thinned by the local mining prediction. Discovery does NOT
	// affect VR visibility.
	void UpdateSegmentVisual(int32 SegmentIndex);

	// Replicated MinedOut arrived from the server -> re-drive the mining visual on the client.
	UFUNCTION()
	void OnRep_MinedOut();

	// The generated visual segments (one per SAMPLE interval); SegmentMeshes[i] spans sample
	// i -> i+1 and is driven by the state at sample i.
	UPROPERTY()
	TArray<USplineMeshComponent*> SegmentMeshes;

	UPROPERTY()
	AGeoRefsManager* GeoRefs = nullptr;

	// Shared runtime form: arc-length-uniform geodetic points (Lon, Lat, Height). Baked
	// locally on every instance (deterministic), so it is NOT replicated.
	UPROPERTY(Transient)
	TArray<FVector> Samples;

	// Detection cache: each sample projected to the reference sphere in UE world space.
	UPROPERTY(Transient)
	TArray<FVector> SamplesSurfaceWorld;

	// SERVER-authoritative discovery, index-aligned with Samples. Monotonic. REPLICATED: the
	// mining client needs it to gate mining locally (only discovered vein is mineable) without
	// prediction divergence. Also feeds the server-side query + bridge.
	UPROPERTY(Replicated)
	TArray<uint8> Discovered;

	// SERVER-authoritative, REPLICATED: per-sample "mined out" flag. Monotonic. Drives the
	// authoritative hide on every client.
	UPROPERTY(ReplicatedUsing = OnRep_MinedOut)
	TArray<uint8> MinedOut;

	// CLIENT-local mining prediction (0..1), index-aligned. Only meaningful on the mining
	// client; gives the smooth thinning + gates the one-shot completion report. Not replicated.
	UPROPERTY(Transient)
	TArray<float> MinedLocal;

	// CLIENT-local bookkeeping: which samples this client already reported as discovered, so
	// each flip is RPC'd exactly once. Not replicated.
	UPROPERTY(Transient)
	TArray<uint8> LocallyReportedDiscovered;
};
