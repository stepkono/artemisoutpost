// Fill out your copyright notice in the Description page of Project Settings.

#include "TransformationsManager.h"
#include "Async/Async.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h"
#include "CesiumGeoreference.h"
#include "CesiumEllipsoid.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "ChaosVehicleMovementComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "GameData/ArtemisGameState.h"
#include "Miscellaneous/GeoUtils.h"
#include "Moon/MapCutoutManager.h"

#pragma region Constructors
void UTransformationsManager::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	
	const UGameInstance* GameInstance = GetWorld()->GetGameInstance();
	
	if (AGameStateBase* DefaultGS = GetWorld()->GetGameState())
	{
		GS = Cast<AArtemisGameState>(DefaultGS);
		
		if (!GS)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to cast to ArtemisGameState."));
			return; 
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Unable to get GameState.")); 
		return; 
	}
	
	GS->OnMapCoordinatesReceived.AddDynamic(this, &UTransformationsManager::OnNewBaseCoordinates);
	
	InitConstants(GameInstance);
}

void UTransformationsManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UTransformationsManager::Deinitialize()
{
	if (AGameStateBase* DefaultGS = GetWorld()->GetGameState())
	{
		GS = Cast<AArtemisGameState>(DefaultGS);
		if (!GS)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to cast to ArtemisGameState."));
			return; 
		}
		
		GS->OnMapCoordinatesReceived.RemoveDynamic(this, &UTransformationsManager::OnNewBaseCoordinates);
	}
	
	Super::Deinitialize();
}

TStatId UTransformationsManager::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTransformationsManager, STATGROUP_Tickables);
}

void UTransformationsManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!bCutoutSet /*|| !Rover*/ || !GeoRef) return;
	
	// Sample params before any transform
	const FTransform MoonNow = GeoRef->GetActorTransform();
	
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
		GeoRef->SetActorRotation(CurrentMoonRotation);
		PreviousMoonRotation = CurrentMoonRotation;
	}

	// ---- Apply scale (WorldToMeters) ----
	/*
	if (IsNewScaleAvailable())
	{
		CurrentMoonVisualScale = CalcInterpolatedScale(DeltaTime);
		TableCenter = CalcNewTableCenter();
		MoveAndExpandCutout();
		UpdateCutout();
		UHeadMountedDisplayFunctionLibrary::SetWorldToMetersScale(GetWorld(), BaseWorldScale * CurrentMoonVisualScale);
	} 
	*/

	// Update moon position 
	GeoRef->SetActorLocation(CalcOffsetMoonOnElevation());
	CurrentMoonPosition = GeoRef->GetActorLocation();
	const FTransform MoonAfter = GeoRef->GetActorTransform();

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
	
	if (!IsNewRotationAvailable() /*&& !IsNewScaleAvailable()*/)
	{
		bNewTransformAvailable = false;
	}
}
#pragma endregion 

#pragma region Initialization 
void UTransformationsManager::InitConstants(const UGameInstance* GameInstance)
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	
	for (TActorIterator<ACesiumGeoreference> const It(World); It;)
	{
		GeoRef = *It;
		break;
	}
	
	if (GeoRef)
	{
		InitialMoonScalingFactor = 1 / GeoRef->GetActorScale3D().X;
		InitialMoonPosition = GeoRef->GetActorLocation();
		CurrentMoonPosition = InitialMoonPosition; 
		MoonRadiusUEUnits = (GeoRef->GetEllipsoid()->GetMaximumRadius() / InitialMoonScalingFactor) * 100.0f;
		InitialMoonRotation = GeoRef->GetActorRotation().Quaternion();

		GeoRef->GetComponentByClass<UMapCutoutManager>()->OnCutoutSet.AddDynamic(this, &UTransformationsManager::HandleCutoutSet); 
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[TransformationManager]: GeoRef not found."))
	}
	
	BaseWorldScale = UHeadMountedDisplayFunctionLibrary::GetWorldToMetersScale(GetWorld()); 
	UE_LOG(LogTemp, Warning, TEXT("Base World scale HMD Lib: %f"), BaseWorldScale);
	
	//XRUtils = GameInstance->GetSubsystem<UXRUtilsSubsystem>();
	CurrentMoonVisualScale = 1.0; 
	CurrentTerrainElevationUEUnits = 0.0f;
	PreviousMoonRotation = FRotator(0.0f, 0.0f, 0.0f);
}

void UTransformationsManager::HandleCutoutSet()
{
	bCutoutSet = true;
}

void UTransformationsManager::SetSpatialAnchors(const FVector AnchorA, const FVector AnchorB, const FVector AnchorC, const FVector AnchorD)
{
	SpatialAnchors.InitialAnchorA = AnchorA; 
	SpatialAnchors.InitialAnchorB = AnchorB;
	SpatialAnchors.InitialAnchorC = AnchorC;
	SpatialAnchors.InitialAnchorD = AnchorD;
	
	VisualAnchorA = AnchorA;
	VisualAnchorB = AnchorB;
	VisualAnchorC = AnchorC;
	VisualAnchorD = AnchorD;
	
	TableCenter = (AnchorA + AnchorB + AnchorC + AnchorD) / 4.0f;
	
	// Moves the moon to the table center
	OffsetMoonOnInitialSet();
	// Sets rover position based on table center
	SetRoverOnInitialSet();
	
	// Update config
	/*
	UApplicationConfig::ClearAllAnchors();
	UApplicationConfig::SaveAnchor(VisualAnchorA); 
	UApplicationConfig::SaveAnchor(VisualAnchorB); 
	UApplicationConfig::SaveAnchor(VisualAnchorC); 
	UApplicationConfig::SaveAnchor(VisualAnchorD); 
	*/
	
	bCutoutSet = true; 
}

void UTransformationsManager::OffsetMoonOnInitialSet()
{
	const FVector MoonOffset = (TableZ * -1) * (173806785 / InitialMoonScalingFactor);
	const FVector SpawnMoonPosition = TableCenter + MoonOffset;
	const FRotator SpawnMoonRotation = FRotator(56, 7, 0);
	
	// Set new moon position and rotation
	CurrentMoonPosition = SpawnMoonPosition;
	GeoRef->SetActorLocation(SpawnMoonPosition);
	GeoRef->SetActorRotation(SpawnMoonRotation);
	PreviousMoonRotation = SpawnMoonRotation;
}

void UTransformationsManager::SetRoverOnInitialSet()
{
	const FVector InitialRoverPos = TableCenter + FVector(0, 0, 700);
	Rover->SetActorLocation(InitialRoverPos, false, nullptr, ETeleportType::ResetPhysics);
	
	const FTransform MoonTransform = GeoRef->GetActorTransform();
	// InitialRoverLocalPos = MoonTransform.InverseTransformPosition(InitialRoverPos);
	RoverInitialLocalPositionFallback = MoonTransform.InverseTransformPosition(InitialRoverPos);
	RoverInitialLocalRotationFallback = MoonTransform.GetRotation().Inverse() * Rover->GetActorQuat();

	bRoverLocalInitialized = false;

	if (Plane)
	{
		Plane->SetActorLocation(TableCenter);
	}
}

void UTransformationsManager::SetTargetTableFrame(const FMatrix TableFrame)
{
	TargetFrame = TableFrame;
	TableZ = TableFrame.GetUnitAxis(EAxis::Z); 
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

void UTransformationsManager::SetAnchorsCollection(UMaterialParameterCollection* MaterialParameterCollection)
{
	AnchorsCollection = MaterialParameterCollection;
}
#pragma endregion

#pragma region Processing
void UTransformationsManager::OnNewBaseCoordinates(FMapBaseCoordinates& MapBaseCoordinates)
{
	if (!bCutoutSet /*|| !bRoverLocalInitialized*/)
	{
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
		
		// Rotation 
		const FMatrix RotationMatrix = BuildRotationMatrix(UpLeftPoint, BottomLeftPoint, BottomRightPoint);
		const FQuat AlignmentQuat = UGeoUtils::BuildQuatFromMatrix(RotationMatrix);
		TargetMoonRotation = (AlignmentQuat * GeoRef->GetActorRotation().Quaternion()).Rotator();
		
		// Scale
		/*
		TargetRelativeMoonScale = CalcTargetRelativeMoonScale(UpLeftPoint, BottomLeftPoint); 
		TargetAbsoluteMoonScale = CalcTargetAbsoluteMoonScale(); 
		*/
		/*----------------------------------------------------*/
		
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
	return FMath::RInterpTo(GeoRef->GetActorRotation(), TargetMoonRotation, DeltaTime, 5); 
}

bool UTransformationsManager::IsNewRotationAvailable() const 
{
	return !GeoRef->GetActorRotation().Equals(TargetMoonRotation, 0.01f); 
}

FMatrix UTransformationsManager::BuildRotationMatrix(const FVector &UpLeft, const FVector &BottomLeft, const FVector &BottomRight) 
{
	const FVector AxisX = UpLeft - BottomLeft;
	const FVector AxisY = BottomRight - BottomLeft;
	
	const FMatrix SourceFrame = UGeoUtils::BuildMatrixFromVectors(AxisX, AxisY);
	UpdateTargetFrame(); 
	const FMatrix RotationMatrix = UGeoUtils::CalculateRotationMatrix(SourceFrame, TargetFrame);
	
	return RotationMatrix;
}

void UTransformationsManager::UpdateTargetFrame() 
{
	if (!AnchorsManager)
	{
		return; 
	}
	
	TArray<AActor*> Anchors = AnchorsManager->GetAnchors();
	
	const FVector AAnchorPos = Anchors[0]->GetActorLocation();
	const FVector BAnchorPos = Anchors[1]->GetActorLocation();
	const FVector DAnchorPos = Anchors[3]->GetActorLocation();
	
	const FVector XAxis = BAnchorPos - AAnchorPos;
	const FVector YAxis = DAnchorPos - AAnchorPos;
	
	const FMatrix UpdatedTargetFrame = UGeoUtils::BuildMatrixFromVectors(XAxis, YAxis);
	
	TableZ = UpdatedTargetFrame.GetUnitAxis(EAxis::Z);
	TargetFrame = UpdatedTargetFrame;
}

FQuat UTransformationsManager::GetLocalRoverRotation(const FTransform& MoonTransform) const
{
	const FQuat LocalRotation = MoonTransform.InverseTransformRotation(Rover->GetActorRotation().Quaternion());
	
	return LocalRotation;
}

void UTransformationsManager::FocusOnRover()
{
	/*
	// Use the authoritative stored moon-local position (not a fresh world->local conversion
	// which could include physics drift)
	const FVector RoverLocalDir = bRoverLocalInitialized
		? InitalRoverLocalPos.GetSafeNormal()
		: GeoRef->GetActorTransform().InverseTransformPosition(Rover->GetActorLocation()).GetSafeNormal();

	// Rotation that maps the rover's local direction to TableZ in world space
	TargetMoonRotation = FQuat::FindBetweenNormals(RoverLocalDir, TableZ).Rotator();
	UE_LOG(LogTemp, Log, TEXT("[FocusOnRover] RoverLocalDir: %s"), *RoverLocalDir.ToString());
	bNewTransformAvailable = true;
	*/
}

/*--------------------------------------------*/
#pragma endregion 

#pragma region SCALE
FVector UTransformationsManager::CalcOffsetMoonOnElevation() const
{
	// TODO: has to be interpolated elevation 
	const float TotalOffset = TargetTerrainElevationUEUnits + MoonRadiusUEUnits; 
	const FVector Direction = TableZ * -1; 
	
	return TableCenter + Direction * TotalOffset;
}

/*
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

void UTransformationsManager::UpdateCutout() const
{
	const FLinearColor NewAnchorA(VisualAnchorA); 
	const FLinearColor NewAnchorB(VisualAnchorB);
	const FLinearColor NewAnchorC(VisualAnchorC);
	const FLinearColor NewAnchorD(VisualAnchorD);
	
	UKismetMaterialLibrary::SetVectorParameterValue(GetWorld(), AnchorsCollection, FName("V1"), NewAnchorA); 
	UKismetMaterialLibrary::SetVectorParameterValue(GetWorld(), AnchorsCollection, FName("V2"), NewAnchorB); 
	UKismetMaterialLibrary::SetVectorParameterValue(GetWorld(), AnchorsCollection, FName("V3"), NewAnchorC); 
	UKismetMaterialLibrary::SetVectorParameterValue(GetWorld(), AnchorsCollection, FName("V4"), NewAnchorD); 
	
	UApplicationConfig::ClearAllAnchors();
	UApplicationConfig::SaveAnchor(VisualAnchorA); 
	UApplicationConfig::SaveAnchor(VisualAnchorB); 
	UApplicationConfig::SaveAnchor(VisualAnchorC); 
	UApplicationConfig::SaveAnchor(VisualAnchorD); 
}

bool UTransformationsManager::IsNewScaleAvailable() const
{
	// TODO: Might be a little more precise
	return !FMath::IsNearlyEqual(CurrentMoonVisualScale, TargetAbsoluteMoonScale, 0.01f); 
}
#pragma endregion
*/

#pragma region Position
/*---------------MOON POSITION---------------*/
FVector UTransformationsManager::GetGeodeticPosition(const FVector& WorldPosition) const
{
	const FVector AbsolutMoonDelta = InitialMoonPosition - GeoRef->GetActorLocation(); 
	const FVector OffsetWorldPos = WorldPosition + AbsolutMoonDelta;
	const FVector LocalToPivot = OffsetWorldPos - InitialMoonPosition;
	const FQuat DeltaRotation = (GeoRef->GetActorRotation().Quaternion().Inverse() * InitialMoonRotation).GetNormalized();
	const FVector RotatedLocal = DeltaRotation.RotateVector(LocalToPivot);
	const FVector FinalPosition = RotatedLocal + InitialMoonPosition;
	const FVector ManualCalResult = GeoRef->TransformUnrealPositionToLongitudeLatitudeHeight(FinalPosition);
	// UE_LOG(LogTemp, Warning, TEXT("Man Rover pos in geodetic: %s"), *ManualCalResult.ToString()); 
	
	const FVector LocalPos = GeoRef->GetActorTransform().InverseTransformPosition(WorldPosition);
	
	return GeoRef->TransformUnrealPositionToLongitudeLatitudeHeight(LocalPos);
}

FVector UTransformationsManager::GeoToUnreal(const FVector& GeoCoordinates) const
{
	const FVector LocalPos = GeoRef->TransformLongitudeLatitudeHeightPositionToUnreal(GeoCoordinates);
	
	return GeoRef->GetActorTransform().TransformPosition(LocalPos);
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
	if (!bHasValidGroundedPos || !GeoRef) return FVector::ZeroVector;
	return GeoRef->GetActorTransform().TransformPosition(LastValidGroundedLocalPos);
}

void UTransformationsManager::ResetRoverSettleState()
{
	if (!Rover || !GeoRef) return;

	const FTransform MoonNow = GeoRef->GetActorTransform();

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
	const FMatrix StaticBasis = UGeoUtils::GetLocalSpatialReferenceFrame(GeoPos, GeoRef);

	// 3. Rotate basis vectors from GeoRef-local to world space
	const FQuat MoonRot = GeoRef->GetActorQuat();
	const FVector N = MoonRot.RotateVector(StaticBasis.GetUnitAxis(EAxis::X));
	const FVector E = MoonRot.RotateVector(StaticBasis.GetUnitAxis(EAxis::Y));
	const FVector U = MoonRot.RotateVector(StaticBasis.GetUnitAxis(EAxis::Z));

	return FMatrix(N, E, U, FVector::ZeroVector);
}
#pragma endregion 