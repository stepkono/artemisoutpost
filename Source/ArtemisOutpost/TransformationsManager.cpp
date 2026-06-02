// Fill out your copyright notice in the Description page of Project Settings.

#include "TransformationsManager.h"
#include "Async/Async.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h"
#include "CesiumGeoreference.h"
#include "CesiumEllipsoid.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "ChaosVehicleMovementComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "GameData/ArtemisGameState.h"
#include "Miscellaneous/GeoUtils.h"
#include "Miscellaneous/XRUtilsSubsystem.h"
#include "Moon/MapCutoutManager.h"

#pragma region Constructors
void UTransformationsManager::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	
}

void UTransformationsManager::Activate(const FVector &InTableCenter, const FVector &InTableNormal)
{
	InitConstants(InTableCenter, InTableNormal);		
}

void UTransformationsManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UTransformationsManager::Deinitialize()
{
	Super::Deinitialize();
}

TStatId UTransformationsManager::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTransformationsManager, STATGROUP_Tickables);
}

void UTransformationsManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!bCutoutSet /*|| !Rover*/ || !ARGeoRef) return;
	
	// Sample params before any transform
	const FTransform MoonNow = ARGeoRef->GetActorTransform();
	
	//GeoRoverPos = GetGeodeticPosition(Rover->GetActorLocation());
	
	// Detect controlled rover movement via control inputs
	/*
	bool bRoverIsControlled = false;
	if (WheeledVehComp)
	{
		const float Throttle = FMath::Abs(WheeledVehComp->GetThrottleInput());
		const float Brake = FMath::Abs(WheeledVehComp->GetBrakeInput());
		bRoverIsControlled = (Throttle > KINDA_SMALL_NUMBER) || (Brake > KINDA_SMALL_NUMBER);
	}
	*/
	// Initialize rover tracking when wheels are on the ground and not being controlled
	/*
	if (!bRoverLocalInitialized)
	{
		if (IsRoverGrounded())
		{
			RoverInitialLocalPos = CaptureRoverLocalPosition(MoonNow);
			RoverInitialLocalRot = MoonNow.InverseTransformRotation(Rover->GetActorRotation().Quaternion());
			bRoverLocalInitialized = true;

			// Also set as first valid grounded position
			LastValidGroundedLocalPos = RoverInitialLocalPos;
			LastValidGroundedLocalRot = RoverInitialLocalRot;
			bHasValidGroundedPos = true;

			UE_LOG(LogTemp, Warning, TEXT("[RoverSettle] GROUNDED | Wheels=%d | Frame=%u | InitLocalPos=%s"),
				WheeledVehComp ? WheeledVehComp->GetNumWheels() : 0, GFrameNumber, *RoverInitialLocalPos.ToString());
		}
	}
	
	
	// Capture rover location when rover is moving
	// TODO: this might stale 
	if (bRoverIsControlled && bRoverLocalInitialized)
	{
		DeltaLocalAbsolute = CaptureRoverLocalPosition(MoonNow) - RoverInitialLocalPos;
		DeltaRoverRotation = (RoverInitialLocalRot.Inverse() * GetLocalRoverRotation(MoonNow));
	}

	// Track last valid grounded position for recovery after clip-through
	if (bRoverLocalInitialized && IsRoverGrounded())
	{
		LastValidGroundedLocalPos = CaptureRoverLocalPosition(MoonNow);
		LastValidGroundedLocalRot = GetLocalRoverRotation(MoonNow);
		bHasValidGroundedPos = true;
	}
	*/

	// ---- Commit the WTM-dependent reposition queued on the PREVIOUS frame ----
	// SetWorldToMetersScale only reframes the camera on the NEXT tracking update, so the moon/table
	// reposition that matches a WTM change is deliberately applied one frame later — in lockstep with
	// the camera actually adopting the new scale. Applying both on the same frame is what made the
	// moon lead the camera by one frame (smooth during the zoom, then a visible "settle" jump when
	// the interpolation stopped and the camera caught up). This block runs ABOVE the early-return
	// gate so a pending commit is never skipped on the frame scaling converges.
	if (bScaleApplyPending)
	{
		TableCenter = PendingTableCenter;
		MoveAndExpandCutout();
		ARGeoRef->SetActorLocation(CalcOffsetMoonOnElevation());
		CurrentMoonPosition = ARGeoRef->GetActorLocation();
		bScaleApplyPending = false;
	}

	if (!bNewTransformAvailable) return;

	// ---- Log A: rover geodetic before moon transform ----
	// UE_LOG(LogTemp, Warning, TEXT("A: Rover pos in geodetic: %s"), *GetGeodeticPosition(Rover->GetActorLocation()).ToString());

	// ---- Apply rotation ----
	if (IsNewRotationAvailable())
	{
		const FRotator CurrentMoonRotation = CalcInterpolatedRotation(DeltaTime);
		//UE_LOG(LogTemp, Warning, TEXT("Current Moon Rotation: %s"), *ARGeoRef->GetActorRotation().ToString());
		//UE_LOG(LogTemp, Warning, TEXT("New Moon Rotation: %s"), *CurrentMoonRotation.ToString());

		ARGeoRef->SetActorRotation(CurrentMoonRotation);
		PreviousMoonRotation = CurrentMoonRotation;
	}

	// ---- Apply scale (WorldToMeters) ----
	// The moon (GeoRef) actor is NOT scaled — rigged pawns live on its surface and cannot be scaled with it.
	// Zoom is done via WorldToMeters scaling: a uniform world scale that keeps the surface in the table plane.
	// We queue the WTM change for end-of-frame and stash the matching reposition (PendingTableCenter);
	// it is committed next frame, when the camera actually adopts the new scale (see commit block above).
	if (IsNewScaleAvailable())
	{
		CurrentMoonVisualScale = CalcInterpolatedScale(DeltaTime);
		// CalcNewTableCenter() records PreviousTableCenter = TableCenter and returns the precompensated
		// center. We do NOT assign TableCenter here — the live TableCenter (and moon position) must keep
		// matching the WTM that is active THIS frame, which was set last frame.
		PendingTableCenter = CalcNewTableCenter();
		UHeadMountedDisplayFunctionLibrary::SetWorldToMetersScale(GetWorld(), BaseWorldScale * CurrentMoonVisualScale);
		bScaleApplyPending = true;
	}
	else
	{
		// No scale change this frame (rotation-only, or after a fresh anchor recalibration):
		// keep the moon glued to the current table center. No WTM change → no latency to pipeline.
		ARGeoRef->SetActorLocation(CalcOffsetMoonOnElevation());
		CurrentMoonPosition = ARGeoRef->GetActorLocation();
	}

	const FTransform MoonAfter = ARGeoRef->GetActorTransform();

	// Update rover transform 
	/*
	if (bRoverLocalInitialized)
	{
		// Position
		// UE_LOG(LogTemp, Warning, TEXT("Delta Absolute: %s"), *DeltaLocalAbsolute.ToString())
		const FVector RoverNewLocalPos = RoverInitialLocalPos + DeltaLocalAbsolute; 
		const FVector RoverNewWorldPos = MoonAfter.TransformPosition(RoverNewLocalPos);
		
		// Rotation 
		const FQuat RoverNewLocalRot = (RoverInitialLocalRot * DeltaRoverRotation).GetNormalized(); 
		const FQuat RoverNewWorldRot = MoonAfter.TransformRotation(RoverNewLocalRot);
		
		// Set
		Rover->SetActorLocationAndRotation(RoverNewWorldPos, RoverNewWorldRot, false, nullptr, ETeleportType::TeleportPhysics);
	}
	else
	{
		const FVector RoverNewWorldPos = MoonAfter.TransformPosition(RoverInitialLocalPositionFallback);
		Rover->SetActorLocationAndRotation(RoverNewWorldPos, RoverInitialLocalRotationFallback, false, nullptr, ETeleportType::TeleportPhysics); 
	}
	
	GeoRoverPos = GetGeodeticPosition(Rover->GetActorLocation());
	*/
	
	// Keep ticking until the final queued reposition has been committed (bScaleApplyPending),
	// otherwise the last WTM change would land with no matching moon move and re-introduce the jump.
	if (!IsNewRotationAvailable() && !IsNewScaleAvailable() && !bScaleApplyPending)
	{
		bNewTransformAvailable = false;
	}
}
#pragma endregion 

#pragma region Initialization 
void UTransformationsManager::InitConstants(const FVector &InTableCenter, const FVector &InTableNormal)
{
	UE_LOG(LogTemp, Log, TEXT("TransformationsManager: Initializing..."));
	
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	
	if (AArtemisGameState* GameState = Cast<AArtemisGameState>(World->GetGameState()))
	{
		GS = GameState; 
		GS->OnMapCoordinatesReceived.AddDynamic(this, &UTransformationsManager::OnNewBaseCoordinates);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("TransformationsManager: Failed to cast to ArtemisGameState"));
		return; 
	}
	
	for (auto GeoRef : TActorRange<ACesiumGeoreference>(World))
	{
		if (GeoRef->ActorHasTag(FName("AR_GEOREF")))
		{
			ARGeoRef = GeoRef;
			break; 
		}
	}
	
	if (ARGeoRef)
	{
		// WorldToMeters scaling: the GeoRef actor scale stays fixed; CurrentMoonVisualScale is the
		// world-scale multiplier that drives the zoom and starts neutral at 1.
		CurrentMoonVisualScale   = 1.0;
		InitialMoonScalingFactor = 1 / ARGeoRef->GetActorScale3D().X;
		InitialMoonPosition      = ARGeoRef->GetActorLocation();
		CurrentMoonPosition      = InitialMoonPosition;
		MoonRadiusUEUnits        = (ARGeoRef->GetEllipsoid()->GetMaximumRadius() / InitialMoonScalingFactor) * 100.0f;
		UE_LOG(LogTemp, Log, TEXT("[TransformationManager]: Ellipsoid radius:        %f"), MoonRadiusUEUnits);
		UE_LOG(LogTemp, Log, TEXT("[TransformationManager]: Ellipsoid radius scaled: %f"), MoonRadiusUEUnits * InitialMoonScalingFactor);
		InitialMoonRotation      = ARGeoRef->GetActorRotation().Quaternion();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[TransformationManager]: GeoRef not found."))
		return;
	}

	if (UAnchorsManagerSubsystem* AMS = World->GetGameInstance()->GetSubsystem<UAnchorsManagerSubsystem>())
	{
		AnchorsManager = AMS;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[TransformationManager]: Unable to find AnchorsManagerSubsystem."))
		return;
	}

	// GameInstanceSubsystem — always instantiated, used to precompensate positions/scale for the WTM change.
	XRUtils = World->GetGameInstance()->GetSubsystem<UXRUtilsSubsystem>();

	// Base WorldToMeters scale that CurrentMoonVisualScale multiplies each tick.
	BaseWorldScale = UHeadMountedDisplayFunctionLibrary::GetWorldToMetersScale(GetWorld());
	UE_LOG(LogTemp, Log, TEXT("[TransformationManager]: Base World scale: %f"), BaseWorldScale);

	// Seed the initial/visual anchors from the live spatial anchors (A=0 BL, B=1 TL, C=2 TR, D=3 BR).
	// InitialAnchor* is the fixed physical table reference used by CalcTargetRelativeMoonScale;
	// VisualAnchor* are the cutout corners that get moved/expanded as the moon zooms.
	TArray<AActor*> Anchors = AnchorsManager->GetAnchors();
	if (Anchors.Num() >= 4)
	{
		SpatialAnchors.InitialAnchorA = Anchors[0]->GetActorLocation();
		SpatialAnchors.InitialAnchorB = Anchors[1]->GetActorLocation();
		SpatialAnchors.InitialAnchorC = Anchors[2]->GetActorLocation();
		SpatialAnchors.InitialAnchorD = Anchors[3]->GetActorLocation();

		VisualAnchorA = SpatialAnchors.InitialAnchorA;
		VisualAnchorB = SpatialAnchors.InitialAnchorB;
		VisualAnchorC = SpatialAnchors.InitialAnchorC;
		VisualAnchorD = SpatialAnchors.InitialAnchorD;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[TransformationManager]: Expected 4 anchors to seed spatial anchors, found %d."), Anchors.Num())
	}

	CurrentTerrainElevationUEUnits = 0.0f;
	PreviousMoonRotation = FRotator(0.0f, 0.0f, 0.0f);

	TableCenter = InTableCenter;
	TableZ      = InTableNormal;

	bCutoutSet = true;
}

void UTransformationsManager::SetRoverOnInitialSet()
{
	const FVector InitialRoverPos = TableCenter + FVector(0, 0, 700);
	Rover->SetActorLocation(InitialRoverPos, false, nullptr, ETeleportType::ResetPhysics);
	
	const FTransform MoonTransform = ARGeoRef->GetActorTransform();
	// InitialRoverLocalPos = MoonTransform.InverseTransformPosition(InitialRoverPos);
	RoverInitialLocalPositionFallback = MoonTransform.InverseTransformPosition(InitialRoverPos);
	RoverInitialLocalRotationFallback = MoonTransform.GetRotation().Inverse() * Rover->GetActorQuat();

	bRoverLocalInitialized = false;

	if (Plane)
	{
		Plane->SetActorLocation(TableCenter);
	}
}

void UTransformationsManager::SetRoverPawn(AWheeledVehiclePawn* RoverPawn)
{
	Rover = RoverPawn; 
	
	if (Rover)
	{
		if (UChaosWheeledVehicleMovementComponent* ValidVehComp = Cast<UChaosWheeledVehicleMovementComponent>(Rover->GetVehicleMovementComponent()))
		{
			WheeledVehComp = ValidVehComp;
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to cast to ChaosVehicleMovementComponent on Rover."));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[TransformationManager]: Rover not set."))
	}
}
#pragma endregion

#pragma region Processing
void UTransformationsManager::OnNewBaseCoordinates(FMapBaseCoordinates& MapBaseCoordinates)
{
	if (!bCutoutSet /*|| !bRoverLocalInitialized*/)
	{
		UE_LOG(LogTemp, Warning, TEXT("TransformationsManager: Map cutout not set. Ignoring new coords."))
		return; 
	}
	
	{
		FScopeLock Lock(&PendingMutex);
		PendingMapPositions.Add(MapBaseCoordinates);
	}
	
	const bool bGameThreadBusy = bProcessingTask.Exchange(true);
	
	if (!bGameThreadBusy)
	{
		AsyncTask(ENamedThreads::GameThread, [this]()
		{
			ProcessBaseCoordinates_GameThread();
		});	
	}
}

void UTransformationsManager::ProcessBaseCoordinates_GameThread()
{
	if (PendingMapPositions.IsEmpty()) return; 

	while (PendingMapPositions.Num() > 0)
	{
		FMapBaseCoordinates LatestBaseCoordinates;   
		{
			FScopeLock Lock(&PendingMutex);
			LatestBaseCoordinates = PendingMapPositions[PendingMapPositions.Num() - 1];
			PendingMapPositions.Empty(); 
		}
		
		// Get the table corners coordinates on the moon surface
		FVector UpLeftPoint = GeoToUnreal(CoordinatesToVector(LatestBaseCoordinates.UpLeft));
		FVector BottomLeftPoint = GeoToUnreal(CoordinatesToVector(LatestBaseCoordinates.BottomLeft));
		FVector BottomRightPoint = GeoToUnreal(CoordinatesToVector(LatestBaseCoordinates.BottomRight));
		
		/*-----------Prepare data for interpolation-----------*/
		// Elevation
		TargetTerrainElevationUEUnits = (LatestBaseCoordinates.TerrainElevation / InitialMoonScalingFactor) * 100.0f;

		FCalibratedData CalibratedData;
		UpdateTargetFrame(CalibratedData);

		// Rotation
		const FMatrix RotationMatrix = BuildRotationMatrix(UpLeftPoint, BottomLeftPoint, BottomRightPoint);
		const FQuat AlignmentQuat = UGeoUtils::BuildQuatFromMatrix(RotationMatrix);
		TargetMoonRotation = (AlignmentQuat * ARGeoRef->GetActorRotation().Quaternion()).Rotator();

		// Scale (WorldToMeters): map the lunar region edge onto the physical table edge.
		TargetRelativeMoonScale = CalcTargetRelativeMoonScale(UpLeftPoint, BottomLeftPoint);
		TargetAbsoluteMoonScale = CalcTargetAbsoluteMoonScale();

		bNewTransformAvailable = true;
	}
	
	// Release the gaming thread
	bProcessingTask.Store(false); 
}
#pragma endregion 

#pragma region ROTATION
/*---------------MOON ROTATION---------------*/
FRotator UTransformationsManager::CalcInterpolatedRotation(const float &DeltaTime) const
{
	return FMath::RInterpTo(ARGeoRef->GetActorRotation(), TargetMoonRotation, DeltaTime, 5); 
}

bool UTransformationsManager::IsNewRotationAvailable() const 
{
	return !ARGeoRef->GetActorRotation().Equals(TargetMoonRotation, 0.01f); 
}

FMatrix UTransformationsManager::BuildRotationMatrix(const FVector &UpLeft, const FVector &BottomLeft, const FVector &BottomRight) 
{
	const FVector AxisX = UpLeft - BottomLeft;
	const FVector AxisY = BottomRight - BottomLeft;
	
	const FMatrix SourceFrame = UGeoUtils::BuildMatrixFromVectors(AxisX, AxisY);
	const FMatrix RotationMatrix = UGeoUtils::CalculateRotationMatrix(SourceFrame, TargetFrame);
	
	return RotationMatrix;
}

bool UTransformationsManager::UpdateTargetFrame(FCalibratedData& CalibratedAnchors) 
{
	if (!AnchorsManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("TransformationsManager: UpdateTargetFrame: Anchors Manager not initialized."))
		return false; 
	}
	
	TArray<AActor*> Anchors = AnchorsManager->GetAnchors();
	
	// B(top-left)     C(top-right)
	// A(bottom-left)  D(bottom-right)
	CalibratedAnchors = UGeoUtils::CalibrateAnchors(
		Anchors[0]->GetActorLocation(),   // A (bottom-left)  
		Anchors[1]->GetActorLocation(),   // B (top-left)    
		Anchors[3]->GetActorLocation()    // D (bottom-right) 
		);
	
	const FVector XAxis = Anchors[1]->GetActorLocation() - Anchors[0]->GetActorLocation(); 
	const FVector YAxis = Anchors[3]->GetActorLocation() - Anchors[0]->GetActorLocation(); 
	
	const FMatrix UpdatedTargetFrame = UGeoUtils::BuildMatrixFromVectors(XAxis, YAxis);
	UE_LOG(LogTemp, Warning, TEXT("Determinant Frame: %f"), UpdatedTargetFrame.Determinant());

	TableCenter = CalibratedAnchors.PlaneCenter;
	TargetFrame = UpdatedTargetFrame;
	// TODO: TableZ can actually be updated 
	// NOTE: TableZ is intentionally NOT updated here.
	// TableZ is the physical table normal used to compute the moon's world-space depth offset
	// (173M+ UE units). Even sub-millimeter anchor jitter amplifies into hundreds-of-meters
	// of position jump at that distance. TableZ is set once in InitConstants from the
	// calibrated plane normal and must stay stable.

	return true; 
}

FQuat UTransformationsManager::GetLocalRoverRotation(const FTransform& MoonTransform) const
{
	const FQuat LocalRotation = MoonTransform.InverseTransformRotation(Rover->GetActorRotation().Quaternion());
	
	return LocalRotation;
}

/*--------------------------------------------*/
#pragma endregion 

#pragma region SCALE
float UTransformationsManager::CalcInterpolatedScale(const float &DeltaTime)
{
	const float InterpolatedVisualScale = FMath::FInterpTo(CurrentMoonVisualScale, TargetAbsoluteMoonScale, DeltaTime, 5);
	// How much the world must be scaled this tick
	RelativeWorldScaleFactor = InterpolatedVisualScale / CurrentMoonVisualScale;
	// XR-positions have to offset by the world-scale factor
	XRUtils->SetScaleFactor(RelativeWorldScaleFactor);

	return InterpolatedVisualScale;
}

float UTransformationsManager::CalcTargetRelativeMoonScale(const FVector &UpLeftPoint, const FVector &BottomLeftPoint)
{
	const float ScaleOnPhysicalMoon = (UpLeftPoint - BottomLeftPoint).Length();
	const float ScaleOnVirtualMoon = ScaleOnPhysicalMoon / CurrentMoonVisualScale;

	//UE_LOG(LogTemp, Log, TEXT("ScaleOnPhysicalMoon: %f"), ScaleOnPhysicalMoon);
	//UE_LOG(LogTemp, Log, TEXT("ScaleOnVirtualMoon: %f"), ScaleOnVirtualMoon);

	// Relative scale factor is a mapping of moon vector to the table edge length
	// TODO: make sure this vector is correct for correct proportional scale and should the acnhors be scaling?
	return ScaleOnVirtualMoon / (SpatialAnchors.InitialAnchorA - SpatialAnchors.InitialAnchorD).Length();
}

float UTransformationsManager::CalcTargetAbsoluteMoonScale()
{
	if (bFirstScalingIsSet)
	{
		return TargetRelativeMoonScale * CurrentMoonVisualScale;
	}

	bFirstScalingIsSet = true;
	return TargetRelativeMoonScale;
}

FVector UTransformationsManager::CalcNewTableCenter()
{
	PreviousTableCenter = TableCenter;
	return XRUtils->GetXRInvariantPosition(TableCenter);
}

void UTransformationsManager::MoveAndExpandCutout()
{
	// Move cutout
	const FVector CutoutOffset = TableCenter - PreviousTableCenter;
	VisualAnchorA += CutoutOffset;
	VisualAnchorB += CutoutOffset;
	VisualAnchorC += CutoutOffset;
	VisualAnchorD += CutoutOffset;

	// Expand cutout
	VisualAnchorA = TableCenter + (VisualAnchorA - TableCenter) * RelativeWorldScaleFactor;
	VisualAnchorB = TableCenter + (VisualAnchorB - TableCenter) * RelativeWorldScaleFactor;
	VisualAnchorC = TableCenter + (VisualAnchorC - TableCenter) * RelativeWorldScaleFactor;
	VisualAnchorD = TableCenter + (VisualAnchorD - TableCenter) * RelativeWorldScaleFactor;
}

bool UTransformationsManager::IsNewScaleAvailable() const
{
	// TODO: Might be a little more precise
	return !FMath::IsNearlyEqual(CurrentMoonVisualScale, TargetAbsoluteMoonScale, 0.01f);
}
#pragma endregion

#pragma region Elevation
FVector UTransformationsManager::CalcOffsetMoonOnElevation() const
{
	// TableCenter is already precompensated by CalcNewTableCenter for the WTM change.
	// The moon actor is not scaled, so the offset uses MoonRadiusUEUnits directly (no actor scale).
	// TODO: has to be interpolated elevation
	const float TotalOffset = TargetTerrainElevationUEUnits + MoonRadiusUEUnits;
	const FVector Direction = TableZ * -1;

	return TableCenter + Direction * TotalOffset;
}
#pragma endregion

#pragma region Position
/*---------------MOON POSITION---------------*/
FVector UTransformationsManager::GetGeodeticPosition(const FVector& WorldPosition) const
{
	const FVector AbsolutMoonDelta = InitialMoonPosition - ARGeoRef->GetActorLocation(); 
	const FVector OffsetWorldPos = WorldPosition + AbsolutMoonDelta;
	const FVector LocalToPivot = OffsetWorldPos - InitialMoonPosition;
	const FQuat DeltaRotation = (ARGeoRef->GetActorRotation().Quaternion().Inverse() * InitialMoonRotation).GetNormalized();
	const FVector RotatedLocal = DeltaRotation.RotateVector(LocalToPivot);
	const FVector FinalPosition = RotatedLocal + InitialMoonPosition;
	const FVector ManualCalResult = ARGeoRef->TransformUnrealPositionToLongitudeLatitudeHeight(FinalPosition);
	// UE_LOG(LogTemp, Warning, TEXT("Man Rover pos in geodetic: %s"), *ManualCalResult.ToString()); 
	
	const FVector LocalPos = ARGeoRef->GetActorTransform().InverseTransformPosition(WorldPosition);
	
	return ARGeoRef->TransformUnrealPositionToLongitudeLatitudeHeight(LocalPos);
}

FVector UTransformationsManager::GeoToUnreal(const FVector& GeoCoordinates) const
{
	const FVector LocalPos = ARGeoRef->TransformLongitudeLatitudeHeightPositionToUnreal(GeoCoordinates);
	
	return ARGeoRef->GetActorTransform().TransformPosition(LocalPos);
}
/*--------------------------------------------*/

/*---------------ROVER POSITION---------------*/
FVector UTransformationsManager::CaptureRoverLocalPosition(const FTransform &MoonTransform)
{
	const FVector RoverCurrentWorldPosition = Rover->GetActorLocation();
	// TODO: Should i rather do InverseTransformPositionNoScale here? 
	const FVector RoverCurrentLocalPosition = MoonTransform.InverseTransformPosition(RoverCurrentWorldPosition);
	
	return RoverCurrentLocalPosition;
}

bool UTransformationsManager::IsRoverGrounded() const
{
	if (!WheeledVehComp) return false;

	int32 ContactCount = 0;
	const int32 NumWheels = WheeledVehComp->GetNumWheels();
	for (int32 i = 0; i < NumWheels; ++i)
	{
		if (WheeledVehComp->GetWheelState(i).bInContact)
		{
			++ContactCount;
		}
	}
	return ContactCount >= MinWheelsGrounded;
}

FVector UTransformationsManager::GetLastValidGroundedWorldPos() const
{
	if (!bHasValidGroundedPos || !ARGeoRef) return FVector::ZeroVector;
	return ARGeoRef->GetActorTransform().TransformPosition(LastValidGroundedLocalPos);
}

void UTransformationsManager::ResetRoverSettleState()
{
	if (!Rover || !ARGeoRef) return;

	const FTransform MoonNow = ARGeoRef->GetActorTransform();

	// Update fallback to last valid grounded position (or current pos)
	if (bHasValidGroundedPos)
	{
		RoverInitialLocalPositionFallback = LastValidGroundedLocalPos;
		RoverInitialLocalRotationFallback = LastValidGroundedLocalRot;
	}
	else
	{
		RoverInitialLocalPositionFallback = MoonNow.InverseTransformPosition(Rover->GetActorLocation());
		RoverInitialLocalRotationFallback = MoonNow.InverseTransformRotation(Rover->GetActorQuat());
	}

	// Clear driving deltas
	DeltaLocalAbsolute = FVector::ZeroVector;
	DeltaRoverRotation = FQuat::Identity;

	// Force-zero inputs to prevent bRoverIsControlled from blocking settling
	if (WheeledVehComp)
	{
		WheeledVehComp->SetThrottleInput(0);
		WheeledVehComp->SetBrakeInput(0);
		WheeledVehComp->SetSteeringInput(0);
		
		Rover->GetMesh()->SetPhysicsLinearVelocity(FVector(0,0,0)); 
		Rover->GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector(0,0,0));
	}

	bRoverLocalInitialized = false;
}
/*--------------------------------------------*/
#pragma endregion

#pragma region Helpers
FVector UTransformationsManager::CoordinatesToVector(const FCoordinates Coordinates) const 
{
	return FVector(Coordinates.Longitude, Coordinates.Latitude, Coordinates.Height);
}

FVector UTransformationsManager::GetGeoRoverPos()
{
	return GeoRoverPos;
}

void UTransformationsManager::GetVisualAnchors(FVector& OutAnchorA, FVector& OutAnchorB, FVector& OutAnchorC, FVector& OutAnchorD) const
{
	OutAnchorA = VisualAnchorA;
	OutAnchorB = VisualAnchorB;
	OutAnchorC = VisualAnchorC;
	OutAnchorD = VisualAnchorD;
}

FMatrix UTransformationsManager::GetWorldSpaceLocalBasis(const FVector& WorldPosition) const
{
	// 1. Get correct geodetic position using wrapper that accounts for moon transform
	const FVector GeoPos = GetGeodeticPosition(WorldPosition);

	// 2. Compute basis in GeoRef-local space (static Cesium methods are valid here)
	const FMatrix StaticBasis = UGeoUtils::GetLocalSpatialReferenceFrame(GeoPos, ARGeoRef);

	// 3. Rotate basis vectors from GeoRef-local to world space
	const FQuat MoonRot = ARGeoRef->GetActorQuat();
	const FVector N = MoonRot.RotateVector(StaticBasis.GetUnitAxis(EAxis::X));
	const FVector E = MoonRot.RotateVector(StaticBasis.GetUnitAxis(EAxis::Y));
	const FVector U = MoonRot.RotateVector(StaticBasis.GetUnitAxis(EAxis::Z));

	return FMatrix(N, E, U, FVector::ZeroVector);
}
#pragma endregion 