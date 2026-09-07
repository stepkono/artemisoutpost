// Fill out your copyright notice in the Description page of Project Settings.


#include "AMasterRover.h"

#include "EngineUtils.h"
#include "PuppetRover.h"
#include "ArtemisOutpost/Moon/Cesium/GeoRefsManager.h"
#include "ArtemisOutpost/Moon/Cesium/GeoTools/GeoUtils.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"

AMasterRover::AMasterRover()
{
	bReplicates = true;
	bAlwaysRelevant = true;
}

void AMasterRover::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}

void AMasterRover::BeginPlay()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[Master] BeginPlay: Failed to get World. Aborting..."));
		return;
	}

	for (TActorIterator<AGeoRefsManager> It(World); It; ++It)
	{
		if (AGeoRefsManager* Manager = *It)
		{
			GeoRefsManager = Manager;
			break;
		}
	}

	if (!GeoRefsManager)
	{
		Super::BeginPlay();
		UE_LOG(LogTemp, Error, TEXT("[Master] BeginPlay: Failed to get GeoRefsManager."));
		return;
	}

	if (AArtemisGameState* GS = Cast<AArtemisGameState>(World->GetGameState()))
	{
		GameState = GS;

		// Bind control handling on the server only. The rover is not possessed by a controller —
		// it's driven by server code (websocket -> GameState -> HandleControls). The Chaos vehicle
		// component only applies input when a controller is present, so relax that requirement.
		if (HasAuthority())
		{
			if (UChaosVehicleMovementComponent* VMC = GetVehicleMovementComponent())
			{
				VMC->SetRequiresControllerForInputs(false);
			}
			GameState->OnControlCommandReceived.AddDynamic(this, &AMasterRover::HandleControls);
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[Master] BeginPlay: Failed to cast/init ArtemisGameState."));
	}

	// Cached for VR-moon-local delta calculations.
	StartLocalPosition_UE = GeoRefsManager->GetVRMoon()->GetTransform().InverseTransformPositionNoScale(GetActorLocation());

	Super::BeginPlay();
}

void AMasterRover::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// ---- Client master: pure replicated-transform carrier ----
	// The client instance must not run its own physics; if it does it free-simulates during the gaps
	// where the resting server body stops replicating movement, sinks under custom gravity, and drags
	// the puppet down. Keep simulation off (re-assert each tick; the Chaos vehicle may re-enable it),
	// then ease the actor toward the latest replicated target (recorded in OnRep_ReplicatedMovement)
	// so it moves smoothly on the VR moon and the puppet copies that motion.
	if (!HasAuthority())
	{
		if (USkeletalMeshComponent* M = GetMesh())
		{
			if (M->IsSimulatingPhysics())
			{
				M->SetSimulatePhysics(false);
			}
		}

		if (bHasRepTarget)
		{
			const FVector CurLoc = GetActorLocation();
			const FQuat   CurRot = GetActorQuat();
			const FQuat   TgtRot = LastRepMoveRotation.Quaternion();
			const bool bSnap = !bClientSmoothingInit || FVector::Dist(CurLoc, LastRepMoveLocation) > ClientSnapDistance;

			const FVector NewLoc = bSnap
				? LastRepMoveLocation
				: FMath::VInterpTo(CurLoc, LastRepMoveLocation, DeltaTime, ClientLocationInterpSpeed);
			const FQuat NewRot = bSnap
				? TgtRot
				: FMath::QInterpTo(CurRot, TgtRot, DeltaTime, ClientRotationInterpSpeed);

			// Must be a teleport: without a flag the engine treats this as a kinematic move and
			// derives a velocity from the per-frame delta, feeding our interpolation back into the
			// solver as a real impulse (a snap would inject a huge one). ResetPhysics on a snap also
			// zeroes any velocity the body still carries; TeleportPhysics keeps it for the smooth case.
			SetActorLocationAndRotation(NewLoc, NewRot, /*bSweep=*/false, nullptr,
				bSnap ? ETeleportType::ResetPhysics : ETeleportType::TeleportPhysics);
			bClientSmoothingInit = true;
		}
	}

	if (!PuppetRover || !GeoRefsManager)
	{
		return;
	}

	// ---- Drive the puppet (client) ----
	// Map the master's geodetic location on the VR moon onto the AR moon, and transfer its pose
	// through the local surface frame (N/E/U) at that lat/long. The master is already smoothed above,
	// so the puppet simply copies the result.
	if (!HasAuthority())
	{
		const FVector MasterWorld         = GetActorLocation();
		const FVector MasterGeoPosition   = GeoRefsManager->UECoordsToVRMoonCoords(MasterWorld);       // world -> VR-moon LLH
		const FVector PuppetWorldPosition = GeoRefsManager->ARMoonCoordsToUECoords(MasterGeoPosition); // LLH -> AR-moon world

		auto SurfaceQuatWorld = [](ACesiumGeoreference* Geo, const FVector& GeoPos) -> FQuat
		{
			const FMatrix LocalBasis = UGeoUtils::GetLocalSpatialReferenceFrame(GeoPos, Geo);
			return Geo->GetActorQuat() * LocalBasis.ToQuat();
		};

		const FQuat MasterQuat = GetActorQuat();
		const FQuat VRSurface  = SurfaceQuatWorld(GeoRefsManager->GetVRMoon(), MasterGeoPosition);
		const FQuat ARSurface  = SurfaceQuatWorld(GeoRefsManager->GetARMoon(), MasterGeoPosition);

		const FQuat MasterPoseRelToSurface = VRSurface.Inverse() * MasterQuat;    // heading/tilt vs the ground
		const FQuat PuppetWorldOrientation = ARSurface * MasterPoseRelToSurface;  // same ground-relative pose on AR moon

		// Refuse to push non-finite values into the puppet (would make it vanish / corrupt movement).
		if (!MasterWorld.ContainsNaN() && !MasterGeoPosition.ContainsNaN()
			&& !PuppetWorldPosition.ContainsNaN() && !PuppetWorldOrientation.ContainsNaN())
		{
			PuppetRover->SetActorLocationAndRotation(PuppetWorldPosition, PuppetWorldOrientation);
		}
	}
}

void AMasterRover::OnRep_ReplicatedMovement()
{
	Super::OnRep_ReplicatedMovement();

	// The client master has physics simulation disabled (see Tick), so Unreal's physics-replication
	// path (bRepPhysics=true, because the SERVER body simulates) no longer moves the actor. Record the
	// incoming transform as the target that Tick eases the master toward, rather than snapping here
	// (which would be choppy at network cadence).
	const FRepMovement& RM = GetReplicatedMovement();
	LastRepMoveLocation = RM.Location;
	LastRepMoveRotation = RM.Rotation;
	bHasRepTarget = true;
}

FVector AMasterRover::GetLocalPos_UE() const
{
	const FVector WorldPos_UE = GetActorLocation();
	return GeoRefsManager->GetVRMoon()->GetTransform().InverseTransformPositionNoScale(WorldPos_UE);
}

APuppetRover* AMasterRover::GetPuppetRover()
{
	return PuppetRover;
}

void AMasterRover::SetGeoRefsManager(AGeoRefsManager* InManager)
{
	GeoRefsManager = InManager;
}

void AMasterRover::HandleControls(FControlCommand& ControlCommand)
{
	// Throttle
	if (ControlCommand.Accelerator > 0.0f)
	{
		GetVehicleMovementComponent()->SetTargetGear(1, true);
		GetVehicleMovementComponent()->SetThrottleInput(ControlCommand.Accelerator);
		GetVehicleMovementComponent()->SetBrakeInput(0.0f);
	}
	else if (ControlCommand.Accelerator < 0.0f)
	{
		GetVehicleMovementComponent()->SetTargetGear(-1, true);
		GetVehicleMovementComponent()->SetThrottleInput(FMath::Abs(ControlCommand.Accelerator));
		GetVehicleMovementComponent()->SetBrakeInput(0.0f);
	}
	else
	{
		GetVehicleMovementComponent()->SetThrottleInput(0.0f);
		GetVehicleMovementComponent()->SetBrakeInput(0.0f);
	}

	// Steering
	this->GetVehicleMovementComponent()->SetSteeringInput(ControlCommand.Steering);
}
