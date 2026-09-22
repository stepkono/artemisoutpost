// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArtemisOutpost/Player/PlayerCues/PlayerCueTypes.h"
#include "MoonVisualisationLayer.generated.h"

class ACesiumGeoreference;
class AGeoRefsManager;

UENUM(BlueprintType)
enum class EBeaconSource : uint8
{
	Pointer UMETA(DisplayName = "Pointer"),
	Gaze    UMETA(DisplayName = "Gaze"),
};

// One beacon = one other player's pointer hit or gaze hit, placed on THIS layer's moon.
USTRUCT(BlueprintType)
struct ARTEMISOUTPOST_API FMoonBeacon
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Moon Layer")
	FString UPID;

	// Colour tag source.
	UPROPERTY(BlueprintReadOnly, Category = "Moon Layer")
	int32 PlayerNumber = -1;

	UPROPERTY(BlueprintReadOnly, Category = "Moon Layer")
	EBeaconSource Source = EBeaconSource::Pointer;

	// The context the pointing player is in (decides laser vs beacon per the Entwurf matrix).
	UPROPERTY(BlueprintReadOnly, Category = "Moon Layer")
	EPlayerContext SourceContext = EPlayerContext::VR;

	UPROPERTY(BlueprintReadOnly, Category = "Moon Layer")
	FPointingTarget Target;

	// The hit re-anchored on this layer's moon, in UE world space, valid this frame.
	UPROPERTY(BlueprintReadOnly, Category = "Moon Layer")
	FVector WorldLocation = FVector::ZeroVector;
};

/**
 * Client-only visual layer glued to ONE moon (AR table moon or VR moon). PULL model: every tick it
 * walks GameState->PlayerArray and, for every OTHER player with a pointer or gaze hit this layer is
 * supposed to show, converts the replicated geo hit to world space with this moon's georeference and
 * raises OnBeaconUpdated; players that stopped get OnBeaconRemoved. Conversion happens at draw time,
 * so beacons stay glued when the AR moon moves or zooms.
 *
 * Which sources are shown is the Entwurf matrix, implemented per child in ShouldShow: the AR layer
 * shows beacons for VR and R pointers (AR pointers are lasers on the AR pawns themselves), the VR
 * layer shows beacons for AR and R pointers.
 *
 * Placed in the level by the designer (one per moon). The visuals are the BP child's job (implement
 * OnBeaconUpdated / OnBeaconRemoved); this base only manages the beacon set. Never replicates.
 */
UCLASS(Abstract, Blueprintable)
class ARTEMISOUTPOST_API AMoonVisualisationLayer : public AActor
{
	GENERATED_BODY()

public:
	AMoonVisualisationLayer();

	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintPure, Category = "Moon Layer")
	ACesiumGeoreference* GetMoon() const { return Moon; }

	// Geo (lon/lat/height) -> UE world on this layer's moon. Zero if the georeference is missing.
	UFUNCTION(BlueprintPure, Category = "Moon Layer")
	FVector GeoToWorld(const FVector& LonLatHeight) const;

	UFUNCTION(BlueprintPure, Category = "Moon Layer")
	TArray<FMoonBeacon> GetActiveBeacons() const;

protected:
	virtual void BeginPlay() override;

	// ---- Per-moon hooks (implemented by the AR / VR child) ----
	virtual ACesiumGeoreference* ResolveMoon(AGeoRefsManager* GeoRefs) const
		PURE_VIRTUAL(AMoonVisualisationLayer::ResolveMoon, return nullptr;);

	virtual FVector GeoToWorldOnMoon(AGeoRefsManager* GeoRefs, const FVector& LonLatHeight) const
		PURE_VIRTUAL(AMoonVisualisationLayer::GeoToWorldOnMoon, return FVector::ZeroVector;);

	// The Entwurf matrix: does this layer show a beacon for a cue coming from that context?
	virtual bool ShouldShow(EPlayerContext SourceContext, EBeaconSource Source) const { return true; }

	// ---- View hooks (implement the visuals in the BP child) ----

	// A beacon appeared or moved / changed target. Called only when something actually changed.
	UFUNCTION(BlueprintNativeEvent, Category = "Moon Layer")
	void OnBeaconUpdated(const FMoonBeacon& Beacon);
	virtual void OnBeaconUpdated_Implementation(const FMoonBeacon& Beacon) {}

	UFUNCTION(BlueprintNativeEvent, Category = "Moon Layer")
	void OnBeaconRemoved(const FString& UPID, EBeaconSource Source);
	virtual void OnBeaconRemoved_Implementation(const FString& UPID, EBeaconSource Source) {}

	// ---- Designer switches ----
	UPROPERTY(EditAnywhere, Category = "Moon Layer")
	bool bShowPointerBeacons = true;

	UPROPERTY(EditAnywhere, Category = "Moon Layer")
	bool bShowGazeMarkers = true;

	// The local player's own cues are normally not shown (they see their own laser).
	UPROPERTY(EditAnywhere, Category = "Moon Layer")
	bool bShowOwnCues = false;

	// Attach this actor to the moon's root on BeginPlay so children authored under it follow the moon.
	UPROPERTY(EditAnywhere, Category = "Moon Layer")
	bool bAttachToMoon = true;

private:
	void SyncFromPlayerStates();
	void Upsert(const FMoonBeacon& Beacon, TSet<FString>& SeenKeys);
	static FString MakeKey(const FString& UPID, EBeaconSource Source);

	UPROPERTY(Transient)
	TObjectPtr<AGeoRefsManager> GeoRefsManager;

	UPROPERTY(Transient)
	TObjectPtr<ACesiumGeoreference> Moon;

	UPROPERTY(Transient)
	TMap<FString, FMoonBeacon> ActiveBeacons;

	bool bAttached = false;
};
