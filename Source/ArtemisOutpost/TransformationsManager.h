// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CesiumGeoreference.h"
#include "WheeledVehiclePawn.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Anchors/AnchorsManagerSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "Containers/Queue.h"
#include "GameData/ArtemisGameState.h"
#include "Miscellaneous/XRUtilsSubsystem.h"
#include "TransformationsManager.generated.h"

USTRUCT(BlueprintType)
struct FSpatialAnchors
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadWrite)
	FVector InitialAnchorA = FVector::ZeroVector; 
	
	UPROPERTY(BlueprintReadWrite)
	FVector InitialAnchorB = FVector::ZeroVector; 
	
	UPROPERTY(BlueprintReadWrite)
	FVector InitialAnchorC = FVector::ZeroVector;
	
	UPROPERTY(BlueprintReadWrite)
	FVector InitialAnchorD = FVector::ZeroVector;
};

UENUM(BlueprintType)
enum class ERoverSettleState : uint8
{
	Placed,   // velocity is 0 but rover is in the air (physics hasn't kicked in)
	Falling,  // velocity > 0, physics is running
	Ready    // velocity dropped back to ~0 after being > 0
};


/**
 * 
 */
UCLASS(BlueprintType, Blueprintable)
class ARTEMISOUTPOST_API UTransformationsManager : public UTickableWorldSubsystem
{
	GENERATED_BODY()
	
#pragma region FUNCTIONS
public: 
#pragma region Constructors
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	
	UFUNCTION(BlueprintCallable, Category="Transformations Manager")
	void Activate(const FVector &InTableCenter, const FVector &InTableNormal); 
	
	UFUNCTION()
	void InitConstants(const FVector &InTableCenter, const FVector &InTableNormal);
#pragma endregion
	
#pragma region Getters
	UFUNCTION(BlueprintCallable)
	FVector GetGeoRoverPos();

	UFUNCTION(BlueprintCallable)
	FMatrix GetWorldSpaceLocalBasis(const FVector& WorldPosition) const;

	UFUNCTION(BlueprintCallable)
	FVector GetLastValidGroundedWorldPos() const;

	// Exposes the moved/expanded cutout corners so MapCutoutManager can read them.
	UFUNCTION(BlueprintCallable)
	void GetVisualAnchors(FVector& OutAnchorA, FVector& OutAnchorB, FVector& OutAnchorC, FVector& OutAnchorD) const;
#pragma endregion

#pragma region Setters
	UFUNCTION(BlueprintCallable)
	void SetRoverPawn(AWheeledVehiclePawn* RoverPawn);
	
	UFUNCTION()
	void ResetRoverSettleState(); 
#pragma endregion
	UPROPERTY()
	AActor* Plane; 
	
private:
#pragma region Processing 
	UFUNCTION()
	void OnNewBaseCoordinates(FMapBaseCoordinates& MapBaseCoordinates);
	
	UFUNCTION()
	void ProcessBaseCoordinates_GameThread();
	
	UFUNCTION()
	FVector CoordinatesToVector(const FCoordinates Coordinates) const; 
	
	//UFUNCTION()
	//void UpdateCutout() const; 
#pragma endregion
	
#pragma region Rotation
	UFUNCTION()
	FMatrix BuildRotationMatrix(const FVector &UpLeft, const FVector &BottomLeft, const FVector &BottomRight);
	
	UFUNCTION()
	bool UpdateTargetFrame(FCalibratedData& CalibratedAnchors); 
	
	UFUNCTION()
	FRotator CalcInterpolatedRotation(const float &DeltaTime) const; 
	
	UFUNCTION()
	bool IsNewRotationAvailable() const; 
	
	UFUNCTION()
	FQuat GetLocalRoverRotation(const FTransform& MoonTransform) const; 
#pragma endregion
	
#pragma region Scale
	UFUNCTION()
	bool IsNewScaleAvailable() const; 
	
	UFUNCTION()
	float CalcInterpolatedScale(const float &DeltaTime);

	UFUNCTION()
	float CalcTargetAbsoluteMoonScale();

	UFUNCTION()
	float CalcTargetRelativeMoonScale(const FVector &UpLeftPoint, const FVector &BottomLeftPoint);

	UFUNCTION()
	FVector CalcNewTableCenter();

	UFUNCTION()
	void MoveAndExpandCutout();
#pragma endregion
	
#pragma region Elevation 
	UFUNCTION()
	FVector CalcOffsetMoonOnElevation() const;
#pragma endregion
	
#pragma region Position
	UFUNCTION()
	void SetRoverOnInitialSet();

	// Idle-only re-seed of TableCenter / cutout corners / moon position straight from the live anchors.
	// Keeps the moon glued to the physical table across a Quest re-localization (headset off/sleep).
	// NOT called during a zoom — see the idle gate in Tick.
	UFUNCTION()
	void RecalibrateFromAnchors();

	UFUNCTION()
	FVector GetGeodeticPosition(const FVector& WorldPosition) const;
	
	UFUNCTION()
	FVector GeoToUnreal(const FVector& GeoCoordinates) const;
	
	UFUNCTION()
	FVector CaptureRoverLocalPosition(const FTransform& MoonTransform);

	bool IsRoverGrounded() const;
#pragma endregion
#pragma endregion
	
#pragma region PROPERTIES
public: 
	UPROPERTY(BlueprintReadOnly)
	FVector TableCenter;
	
	UPROPERTY()
	float MoonRadiusUEUnits;
	
	UPROPERTY()
	AArtemisGameState* GS; 
	
	UPROPERTY()
	UAnchorsManagerSubsystem* AnchorsManager;
private: 
#pragma region Objects
	UPROPERTY()
	ACesiumGeoreference* ARGeoRef;

	UPROPERTY()
	UXRUtilsSubsystem* XRUtils;

	UPROPERTY()
	AWheeledVehiclePawn* Rover;
	
	UPROPERTY()
	UMaterialParameterCollection* AnchorsCollection; 
	
	UPROPERTY()
	UChaosWheeledVehicleMovementComponent* WheeledVehComp = nullptr;
#pragma endregion
	
#pragma region Positions
	UPROPERTY()
	FVector PreviousTableCenter;

	// WTM pipeline: the precompensated table center computed on the frame a WTM change is queued.
	// Committed to TableCenter on the NEXT frame, in lockstep with the camera adopting the new scale.
	UPROPERTY()
	FVector PendingTableCenter = FVector::ZeroVector;

	UPROPERTY()
	FVector CurrentMoonPosition;

	UPROPERTY()
	FVector InitialMoonPosition = FVector::ZeroVector; 
	
	UPROPERTY()
	FVector GeoRoverPos = FVector::ZeroVector;
	
	UPROPERTY()
	FVector LastRoverWorldPosition = FVector::ZeroVector;
#pragma region Scales
	UPROPERTY()
	float TargetMoonScale = 1; 
	
	UPROPERTY()
	float CurrentScalingFactor = 1; 
	
	UPROPERTY()
	float CurrentMoonVisualScale = 1; 
	
	UPROPERTY()
	float TargetAbsoluteMoonScale = 1;
	
	UPROPERTY()
	float TargetRelativeMoonScale = 1; 

	UPROPERTY()
	float RelativeWorldScaleFactor = 1; 
	
	UPROPERTY()
	float BaseWorldScale = 0;

	UPROPERTY()
	float InitialMoonScalingFactor = 1;

	// Physical length of the table X-axis edge in UE units (from live anchors, updated per packet).
	UPROPERTY()
	float CachedTableXAxisLength = 1.0f;

	// Lunar region X-axis length in UE units per unit of GeoRef actor scale
	// (= Src_XAxis.Length() / ActorScale at processing time). Used to reconstruct
	// the current world-space region size at any scale without re-running GeoToUnreal.
	UPROPERTY()
	float BaseGeographicSpan = 1.0f;
#pragma endregion
	
#pragma region Rotation 
	UPROPERTY()
	FRotator TargetMoonRotation = FRotator(0,0,0);
	
	UPROPERTY()
	FRotator PreviousMoonRotation = FRotator(0,0,0);; 
	
	UPROPERTY()
	FQuat InitialMoonRotation = FQuat::Identity; 
#pragma endregion
	
#pragma region Elevation
	UPROPERTY()
	float TargetTerrainElevationUEUnits = 0.0f;
	
	UPROPERTY()
	float CurrentTerrainElevationUEUnits = 0.0f; 
#pragma endregion
	
#pragma endregion
	
#pragma region RoverTracking
public: 
	UPROPERTY(BlueprintReadOnly)
	bool bRoverLocalInitialized = false;
private:
	UPROPERTY()
	FVector RoverInitialLocalPos = FVector::ZeroVector;

	UPROPERTY()
	FQuat RoverInitialLocalRot = FQuat::Identity;

	UPROPERTY()
	FVector DeltaLocalAbsolute = FVector::ZeroVector;

	UPROPERTY()
	FQuat DeltaRoverRotation = FQuat::Identity;

	UPROPERTY()
	FVector RoverInitialLocalPositionFallback = FVector::ZeroVector;

	UPROPERTY()
	FQuat RoverInitialLocalRotationFallback = FQuat::Identity;
	
	// Last valid grounded position for recovery after clip-through
	FVector LastValidGroundedLocalPos = FVector::ZeroVector;
	FQuat LastValidGroundedLocalRot = FQuat::Identity;
	bool bHasValidGroundedPos = false;

	// Minimum wheels in contact to consider rover "grounded"
	static constexpr int32 MinWheelsGrounded = 3;
#pragma endregion

#pragma region Flags
	UPROPERTY()
	bool bFirstScalingIsSet = false;
	
	// ---- BP state variables (names match BP display) ----
	mutable FRWLock StateLock;
	
	mutable FCriticalSection PendingMutex;

	TAtomic<bool> bProcessingTask { false };
	
	UPROPERTY()
	bool bNewTransformAvailable = false;

	UPROPERTY()
	bool bCutoutSet = false;

	// WTM pipeline: true when a WorldToMeters change was queued this frame and its matching
	// moon/table reposition still needs to be committed on the next frame.
	UPROPERTY()
	bool bScaleApplyPending = false;

	// Idle re-seed: anchor[0]'s world position last frame, used to detect when the anchors have
	// actually moved (re-localization) vs. sitting still. Recalibrate only when it changed.
	UPROPERTY()
	FVector LastAnchorPos = FVector::ZeroVector;

	UPROPERTY()
	bool bHasLastAnchorPos = false;

	// Min per-frame anchor movement (UE units) to count as a real change vs float/tracking noise.
	// Jitter is assumed invisible; raise this if idle shimmer appears at high zoom.
	static constexpr float AnchorMoveThreshold = 1.0f;
#pragma endregion
	
	UPROPERTY()
	TArray<FMapBaseCoordinates> PendingMapPositions;
	
#pragma region Table
	UPROPERTY()
	FVector VisualAnchorA = FVector::ZeroVector; 
	
	UPROPERTY()
	FVector VisualAnchorB = FVector::ZeroVector;
	
	UPROPERTY()
	FVector VisualAnchorC = FVector::ZeroVector; 
	
	UPROPERTY()
	FVector VisualAnchorD = FVector::ZeroVector; 
	
	UPROPERTY()
	FSpatialAnchors SpatialAnchors;
	
	UPROPERTY()
	FMatrix TargetFrame; 
	
	UPROPERTY()
	FVector TableZ = FVector::ZeroVector; 
#pragma endregion
#pragma endregion
};
