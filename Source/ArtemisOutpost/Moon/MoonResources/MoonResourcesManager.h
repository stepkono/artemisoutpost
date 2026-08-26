// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ArtemisOutpost/Miscellaneous/DataTypes.h"  // ERessourceType
#include "MoonResourcesManager.generated.h"

class UResourceVeinSpline;
class APawnController;

// Network seam: re-broadcast of a vein's per-sample deltas, tagged with the source vein.
// Carries GEODETIC positions (Lon, Lat, Height). The bridge subscribes here later. Server-side.
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnVeinDelta, UResourceVeinSpline* /*Vein*/, const TArray<FVector>& /*GeoPositions*/);

/**
 * Owns the registry of resource veins and is the single entry point for the resource domain.
 * Holds REFERENCES, not a copy of vein state.
 *
 * Flow (variant A):
 *   CLIENT: the scanning/mining tool calls ClientReportScan/ClientReportMining each tick.
 *   Detection is done locally on the veins; only per-sample FLIP events are forwarded to the
 *   server via the local APawnController's Server RPCs (event-gated, never per-tick streams).
 *   SERVER: those RPCs call ServerReportDiscovered/ServerReportMined here, which apply the
 *   authoritative state on the vein and (via the vein hooks) feed the bridge seam.
 */
UCLASS()
class ARTEMISOUTPOST_API UMoonResourcesManager : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// Veins self-register at BeginPlay / unregister at EndPlay.
	void RegisterVein(UResourceVeinSpline* Vein);
	void UnregisterVein(UResourceVeinSpline* Vein);

	// CLIENT: detect newly-covered vein samples locally and report each flip to the server
	// through the local PawnController. No-op on samples already reported by this client.
	void ClientReportScan(const FVector& HitWorld, float RadiusWorld);
	void ClientReportMining(const FVector& HitWorld, float RadiusWorld, float DeltaSeconds, float RatePerSecond);

	// SERVER: apply reported flips authoritatively (called from APawnController's Server RPCs).
	void ServerReportDiscovered(UResourceVeinSpline* Vein, const TArray<int32>& Indices);
	void ServerReportMined(UResourceVeinSpline* Vein, const TArray<int32>& Indices);

	// Read-side query (SERVER): resource type of the first DISCOVERED vein under GeoPos, else None.
	ERessourceType GetResourceTypeAt(const FVector& GeoPos, float QueryRadiusWorld) const;

	// Network seam — bridge subscribes here later.
	FOnVeinDelta OnVeinDiscovered;
	FOnVeinDelta OnVeinMined;

private:
	// The local PawnController (client) used to originate the Server RPCs.
	APawnController* GetLocalPawnController() const;

	// Forward a vein's own hooks up to the subsystem-level (vein-tagged) delegates.
	void HandleVeinDiscovered(const TArray<FVector>& GeoPositions, UResourceVeinSpline* Vein);
	void HandleVeinMined(const TArray<FVector>& GeoPositions, UResourceVeinSpline* Vein);

	UPROPERTY()
	TArray<UResourceVeinSpline*> Veins;
};
