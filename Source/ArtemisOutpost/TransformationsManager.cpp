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
	bool bRoverIsControlled = false;
	if (WheeledVehComp)
	{
		const float Throttle = FMath::Abs(WheeledVehComp->GetThrottleInput());
		const float Brake = FMath::Abs(WheeledVehComp->GetBrakeInput());
		bRoverIsControlled = (Throttle > KINDA_SMALL_NUMBER) || (Brake > KINDA_SMALL_NUMBER);
	}
	
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
	if (!bNewTransformAvailable) return;

	// ---- Log A: rover geodetic before moon transform ----
	// UE_LOG(LogTemp, Warning, TEXT("A: Rover pos in geodetic: %s"), *GetGeodeticPosition(Rover->GetActorLocation()).ToString());

	// ---- Apply rotation ----
	if (IsNewRotationAvailable())
	{
		const FRotator CurrentMoonRotation = CalcInterpolatedRotation(DeltaTime);
		UE_LOG(LogTemp, Warning, TEXT("Current Moon Rotation: %s"), *ARGeoRef->GetActorRotation().ToString());
		UE_LOG(LogTemp, Warning, TEXT("New Moon Rotation: %s"), *CurrentMoonRotation.ToString());
		
		ARGeoRef->SetActorRotation(CurrentMoonRotation);
		PreviousMoonRotation = CurrentMoonRotation;
	}
	
	if (IsNewScaleAvailable())
	{
		CurrentMoonVisualScale = CalcInterpolatedScale(DeltaTime);
		ARGeoRef->SetActorScale3D(FVector(CurrentMoonVisualScale));
	}

	// Update moon position 
	//TODO: not sure if the location will be set correctly since the actor is a child 
	ARGeoRef->SetActorLocation(CalcOffsetMoonOnElevation());
	CurrentMoonPosition = ARGeoRef->GetActorLocation();
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
	
	if (!IsNewRotationAvailable() && !IsNewScaleAvailable())
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
		InitialMoonScalingFactor = 1 / ARGeoRef->GetActorScale3D().X;
		InitialMoonPosition = ARGeoRef->GetActorLocation();
		CurrentMoonPosition = InitialMoonPosition; 
		MoonRadiusUEUnits = (ARGeoRef->GetEllipsoid()->GetMaximumRadius() / InitialMoonScalingFactor) * 100.0f;
		InitialMoonRotation = ARGeoRef->GetActorRotation().Quaternion();
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
	
	BaseWorldScale = UHeadMountedDisplayFunctionLibrary::GetWorldToMetersScale(GetWorld()); 
	UE_LOG(LogTemp, Warning, TEXT("Base World scale HMD Lib: %f"), BaseWorldScale);
	
	//XRUtils = GameInstance->GetSubsystem<UXRUtilsSubsystem>();
	CurrentMoonVisualScale = 1.0; 
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
		TargetTerrainElevationUEUnits = (LatestBaseCoordinates.TerrainElevation / InitialMoonScalingFactor) * 100;
		
		FCalibratedData CalibratedData;
		UpdateTargetFrame(CalibratedData); 
		
		// Rotation 
		const FMatrix RotationMatrix = BuildRotationMatrix(UpLeftPoint, BottomLeftPoint, BottomRightPoint);
		const FQuat AlignmentQuat = UGeoUtils::BuildQuatFromMatrix(RotationMatrix);
		TargetMoonRotation = (AlignmentQuat * ARGeoRef->GetActorRotation().Quaternion()).Rotator();
		
		// Scale
		const FVector AR_XAxis  = CalibratedData.BAnchorPos - CalibratedData.AAnchorPos;
		const FVector Src_XAxis = UpLeftPoint - BottomLeftPoint;
		CurrentScalingFactor = AR_XAxis.Length() / Src_XAxis.Length();
		TargetMoonScale = CurrentScalingFactor * ARGeoRef->GetActorScale().X;
		
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
	TableZ      = UpdatedTargetFrame.GetUnitAxis(EAxis::Z);
	TargetFrame = UpdatedTargetFrame;
	
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
FVector UTransformationsManager::CalcOffsetMoonOnElevation() const
{
	const float ScaleRatio = ARGeoRef->GetActorScale().X * InitialMoonScalingFactor;
	const float TotalOffset = (MoonRadiusUEUnits + TargetTerrainElevationUEUnits) * ScaleRatio;
	const FVector Direction = TableZ * -1;

	return TableCenter + Direction * TotalOffset;
}

float UTransformationsManager::CalcInterpolatedScale(const float &DeltaTime)
{
	const float InterpolatedVisualScale = FMath::FInterpTo(CurrentMoonVisualScale, TargetMoonScale, DeltaTime, 5);
	return InterpolatedVisualScale;
}

bool UTransformationsManager::IsNewScaleAvailable() const
{
	// TODO: Might be a little more precise
	return !FMath::IsNearlyEqual(ARGeoRef->GetActorScale().X, TargetMoonScale, 0.01f); 
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