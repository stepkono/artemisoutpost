// Fill out your copyright notice in the Description page of Project Settings.


#include "AMasterRover.h"

#include "AudioMixerBlueprintLibrary.h"
#include "Cesium3DTileset.h"
#include "EngineUtils.h"
#include "PuppetRover.h"
#include "ArtemisOutpost/Cesium/GeoRefsManager.h"
#include "ArtemisOutpost/Miscellaneous/GeoUtils.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"

AMasterRover::AMasterRover()
{
	bReplicates = true;
	bAlwaysRelevant = true;
}

void AMasterRover::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	// Call the Super
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
}

void AMasterRover::BeginPlay()
{
	const TCHAR* Net = HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT");
	UE_LOG(LogTemp, Warning, TEXT("[Master][%s] BeginPlay: %s | World=%s"),
		Net, *GetName(), *GetActorLocation().ToString());

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[Master][%s] BeginPlay: Failed to get World. Aborting..."), Net);
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
		UE_LOG(LogTemp, Error, TEXT("[Master][%s] BeginPlay: Failed to get GeoRefsManager."), Net);
		return;
	}
	
	AArtemisGameState* GS = Cast<AArtemisGameState>(GetWorld()->GetGameState());
	if (GS)
	{
		GameState = GS;
		
		// Only bind on server
		if (HasAuthority())
		{
			// The rover is NOT possessed by a controller — it's driven entirely by server code
			// (websocket -> GameState -> HandleControls -> SetThrottleInput). By default the Chaos
			// vehicle movement component only applies input when the pawn has a controller, and
			// zeroes it otherwise. Turn that requirement off so our programmatic input is honoured.
			
			//TODO: this should probably apply to both server AND client
			if (UChaosVehicleMovementComponent* VMC = GetVehicleMovementComponent())
			{
				VMC->SetRequiresControllerForInputs(false);
			}

			UE_LOG(LogTemp, Log, TEXT("MasterRover: Bind the controls handling."))
			GameState->OnControlCommandReceived.AddDynamic(this, &AMasterRover::HandleControls);
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("MasterRover: Failed to cast and init Game State."));
	}

	UE_LOG(LogTemp, Warning, TEXT("[Master][%s] BeginPlay: GeoRefsManager OK | VRMoon=%s | ARMoon=%s"),
		Net, *GetNameSafe(GeoRefsManager->GetVRMoon()), *GetNameSafe(GeoRefsManager->GetARMoon()));

	// [BasketA] On the client, resolve the VR-moon tileset (the one the master drives on) so Tick
	// can report whether it is actually streaming collision here. The server keeps it resident via
	// GeorefServerLogic; the client does NOT, so this is where fall-through is expected to originate.
	if (!HasAuthority())
	{
		for (TActorIterator<ACesium3DTileset> It(World); It; ++It)
		{
			if (It->ActorHasTag(FName("DEFAULT_TILESET")))
			{
				ClientVRTileset = *It;
				break;
			}
		}
		UE_LOG(LogTemp, Warning, TEXT("[BasketA][CLIENT] BeginPlay: ClientVRTileset=%s (by DEFAULT_TILESET tag)"),
			*GetNameSafe(ClientVRTileset));
	}

	// Initialize the start position to calc the delta vector in next frames
	const FVector WorldPos = this->GetActorLocation();
	StartLocalPosition_UE  = GeoRefsManager->GetVRMoon()->GetTransform().InverseTransformPositionNoScale(WorldPos);
	UE_LOG(LogTemp, Warning, TEXT("[Master][%s] BeginPlay: WorldPos=%s | StartLocalPos_VR=%s"),
		Net, *WorldPos.ToString(), *StartLocalPosition_UE.ToString());
	
	Super::BeginPlay(); 
}

void AMasterRover::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	// Throttle the per-frame logs to roughly once every LogIntervalSeconds.
	LogTimeAccumulator += DeltaTime;
	bool bLogThisFrame = false;
	if (LogTimeAccumulator >= LogIntervalSeconds)
	{
		LogTimeAccumulator -= LogIntervalSeconds;
		bLogThisFrame = true;
	}
	const TCHAR* Net = HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT");

	// ---- [BasketA FIX] Client master is a pure replicated-transform carrier ----
	// The client instance must NOT run its own physics. When it does, it free-simulates during the
	// gaps where the server sends no movement updates (the server body sleeps at rest -> movement
	// replication goes quiet -> the client keeps simulating under custom gravity and sinks, dragging
	// the puppet through the surface; a later server update snaps it back — see the RepDiv logs).
	// With simulation off the client simply holds the server's last replicated transform, which is
	// correct, and the body drops out of the custom-gravity dynamic-particle list. Re-assert every
	// tick because the Chaos vehicle movement component may otherwise re-enable simulation.
	if (!HasAuthority())
	{
		if (USkeletalMeshComponent* M = GetMesh())
		{
			if (M->IsSimulatingPhysics())
			{
				M->SetSimulatePhysics(false);
			}
		}

		// Smooth the client master toward the latest replicated transform (recorded in
		// OnRep_ReplicatedMovement). Easing here — rather than snapping in OnRep — makes the master
		// itself move smoothly on the VR moon (what a VR user sees), and the puppet copies it. Snap
		// on the first placement and on large jumps (teleport / respawn / big correction).
		if (bHasRepTarget)
		{
			const FVector CurLoc = GetActorLocation();
			const FQuat   CurRot = GetActorQuat();
			const FQuat   TgtRot = LastRepMoveRotation.Quaternion();
			const bool bSnap = !bClientSmoothingInit
				|| FVector::Dist(CurLoc, LastRepMoveLocation) > ClientSnapDistance;

			const FVector NewLoc = bSnap
				? LastRepMoveLocation
				: FMath::VInterpTo(CurLoc, LastRepMoveLocation, DeltaTime, ClientLocationInterpSpeed);
			const FQuat NewRot = bSnap
				? TgtRot
				: FMath::QInterpTo(CurRot, TgtRot, DeltaTime, ClientRotationInterpSpeed);

			SetActorLocationAndRotation(NewLoc, NewRot);
			bClientSmoothingInit = true;
		}
	}

	// ---- Position / jitter tracking (server authority, throttled log) ----
	// We measure |dPos| EVERY tick but only LOG the peak on the throttled cadence (no spam).
	// A large MaxTickJitter while LinVel is small means the body is being teleport-corrected by
	// collision (penetration resolution) frame-to-frame rather than moving under real physics.
	if (HasAuthority())
	{
		const FVector P = GetActorLocation();
		if (bHasLastTickPos)
		{
			MaxTickDelta = FMath::Max(MaxTickDelta, (P - LastTickPos).Size());
		}
		LastTickPos = P;
		bHasLastTickPos = true;
	}

	// ---- Server-side collision probe ----
	// Physics for the master rover is authoritative on the server, so the only collision
	// that matters for "falling through" is the server's. Trace straight down (toward the
	// VR moon centre) and report whether a Cesium physics mesh is actually present beneath
	// the rover. A MISS means tiles aren't streamed on the server, the tileset has no
	// physics meshes, or collision is disabled — any of which causes the fall-through.
	if (HasAuthority() && bLogThisFrame && GeoRefsManager && GeoRefsManager->GetVRMoon())
	{
		const FVector RoverPos   = GetActorLocation();
		const FVector MoonCenter = GeoRefsManager->GetVRMoon()->GetActorLocation();
		const FVector DownDir    = (MoonCenter - RoverPos).GetSafeNormal();
		const FVector TraceEnd   = RoverPos + DownDir * 5000000.0f;

		FHitResult Hit;
		// bTraceComplex = true: Cesium tiles only have per-triangle (complex) collision.
		FCollisionQueryParams Params(FName(TEXT("RoverGroundProbe")), /*bTraceComplex=*/true, this);
		const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, RoverPos, TraceEnd, ECC_WorldStatic, Params);

		if (bHit)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Master][SERVER] GroundProbe HIT: Actor=%s Comp=%s Dist=%.1f"),
				*GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()), Hit.Distance);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[Master][SERVER] GroundProbe MISS: no collision under rover (tiles not streamed / no physics mesh / collision channel mismatch)."));
		}
	}

	// ---- Server-side physics-state probe ----
	// Diagnoses the "falls through then spins violently" behaviour:
	//  - LinVel direction vs world-down vs moon-centre-down reveals whether gravity is
	//    misaligned (default world -Z) instead of pointing at the moon centre.
	//  - AngVel magnitude shows the violent spin directly.
	//  - Wheel contact count shows whether the Chaos vehicle thinks it's grounded.
	//  - |WorldPos| shows how far from origin we are (float precision degrades badly past ~1e5).
	if (HasAuthority() && bLogThisFrame)
	{
		const FVector WorldPos = GetActorLocation();
		const FVector LinVel   = GetVelocity();
		const USkeletalMeshComponent* MeshComp = GetMesh();
		const FVector AngVel   = MeshComp ? MeshComp->GetPhysicsAngularVelocityInDegrees() : FVector::ZeroVector;

		FVector MoonDownDir = FVector::DownVector;
		if (GeoRefsManager && GeoRefsManager->GetVRMoon())
		{
			MoonDownDir = (GeoRefsManager->GetVRMoon()->GetActorLocation() - WorldPos).GetSafeNormal();
		}
		const FVector VelDir          = LinVel.GetSafeNormal();
		const float DotWorldDown       = FVector::DotProduct(VelDir, FVector(0.f, 0.f, -1.f)); // ~1 => falling along world -Z (default gravity, WRONG on a globe)
		const float DotMoonDown        = FVector::DotProduct(VelDir, MoonDownDir);               // ~1 => falling toward moon centre (correct gravity)

		int32 WheelsInContact = 0, NumWheels = 0;
		if (UChaosWheeledVehicleMovementComponent* VMC = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent()))
		{
			NumWheels = VMC->GetNumWheels();
			for (int32 i = 0; i < NumWheels; ++i)
			{
				if (VMC->GetWheelState(i).bInContact)
				{
					++WheelsInContact;
				}
			}
		}

		UE_LOG(LogTemp, Warning,
			TEXT("[Master][SERVER] Phys | |WorldPos|=%.0f | LinVel=%.1f (worldDown=%.2f moonDown=%.2f) | AngVel(deg/s)=%.1f %s | Wheels=%d/%d | Rot=%s"),
			WorldPos.Size(), LinVel.Size(), DotWorldDown, DotMoonDown,
			AngVel.Size(), *AngVel.ToString(), WheelsInContact, NumWheels, *GetActorRotation().ToString());
	}

	// ---- Mesh physics-body diagnostic ----
	// Confirms whether the VehicleMesh is actually a simulating dynamic rigid body. Chaos only
	// creates a dynamic particle (the thing FCustomGravityAsyncCallback iterates and applies moon
	// gravity to) when the mesh has a valid body AND is simulating physics. If SimPhysics=0,
	// ValidBody=0, or HasPhysAsset=0, this rover has no dynamic particle and the custom gravity
	// silently skips it — i.e. it "doesn't have physics". Not gated on authority so it reports on
	// whichever instance is running.
	if (bLogThisFrame)
	{
		if (USkeletalMeshComponent* M = GetMesh())
		{
			const FBodyInstance* BI = M->GetBodyInstance();
			// COMLocalOffset = center of mass expressed in the rover's own frame. For the suspension
			// to be valid this must sit inside the wheel footprint (roughly centred, low). A large
			// X/Y offset here is what triggers "Spring configuration is invalid".
			const FVector COMLocalOffset = GetActorTransform().InverseTransformPosition(M->GetCenterOfMass());
			// GravityEnabled + world GravityZ: if GravityEnabled=1 the body still receives default
			// world -Z gravity, which the custom moon-attractor callback only ADDS to (never replaces).
			// World -Z (~-980) dwarfs moon gravity (~162), so the rover falls world-down -> must disable.
			UE_LOG(LogTemp, Warning,
				TEXT("[Master][%s] MeshPhys | SimPhysics=%d Awake=%d ValidBody=%d HasPhysAsset=%d CollEnabled=%d | Mass=%.1f COMLocal=%s | GravityEnabled=%d WorldGravityZ=%.1f"),
				Net, M->IsSimulatingPhysics() ? 1 : 0, M->RigidBodyIsAwake() ? 1 : 0,
				(BI && BI->IsValidBodyInstance()) ? 1 : 0,
				(M->GetPhysicsAsset() != nullptr) ? 1 : 0,
				(int32)M->GetCollisionEnabled(),
				M->GetMass(), *COMLocalOffset.ToString(),
				M->IsGravityEnabled() ? 1 : 0, GetWorld() ? GetWorld()->GetGravityZ() : 0.f);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[Master][%s] MeshPhys | GetMesh() returned null"), Net);
		}
	}

	// ---- [BasketA] Client-side streaming / replication-divergence probe ----
	// Basket A hypothesis: on the client the master free-simulates on a VR moon whose collision is
	// NOT kept resident (GeorefServerLogic runs server-only; the proxy camera may not drive client
	// streaming). Runs on the client only, before the puppet early-return so it reports even if the
	// puppet is missing. Three orthogonal signals:
	//   * GroundProbe   - is there collision under the client master right now? (client MISS while the
	//                     server logs a HIT at the same time => client streaming/collision is the cause)
	//   * RepDiv        - how far the local simulated transform has drifted from the last server
	//                     correction, and how stale that correction is (stale age => corrections stalled)
	//   * Tileset       - client VR tileset culling flags + load progress (expect culling ON, streaming starved)
	if (!HasAuthority() && bLogThisFrame && GeoRefsManager && GeoRefsManager->GetVRMoon())
	{
		const FVector RoverPos   = GetActorLocation();
		const FVector MoonCenter = GeoRefsManager->GetVRMoon()->GetActorLocation();
		const FVector DownDir    = (MoonCenter - RoverPos).GetSafeNormal();
		const FVector TraceEnd   = RoverPos + DownDir * 5000000.0f;

		FHitResult Hit;
		FCollisionQueryParams Params(FName(TEXT("BasketAClientGroundProbe")), /*bTraceComplex=*/true, this);
		const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, RoverPos, TraceEnd, ECC_WorldStatic, Params);
		if (bHit)
		{
			UE_LOG(LogTemp, Warning, TEXT("[BasketA][CLIENT] GroundProbe HIT: Actor=%s Comp=%s Dist=%.1f"),
				*GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()), Hit.Distance);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[BasketA][CLIENT] GroundProbe MISS: NO collision under client master (VR tiles not streamed on client)."));
		}

		// Local (client-simulated) transform vs the last server correction.
		const double Now = GetWorld()->GetTimeSeconds();
		const double Age = (LastRepMoveWorldTime >= 0.0) ? (Now - LastRepMoveWorldTime) : -1.0;
		const float  Divergence = (RoverPos - LastRepMoveLocation).Size();
		const USkeletalMeshComponent* M = GetMesh();
		UE_LOG(LogTemp, Warning,
			TEXT("[BasketA][CLIENT] RepDiv | LocalLoc=%s | LastServerLoc=%s | Divergence=%.1f | RepUpdates=%d LastCorrAge=%.2fs | SimPhys=%d Awake=%d"),
			*RoverPos.ToString(), *LastRepMoveLocation.ToString(), Divergence,
			RepMoveUpdateCount, Age,
			(M && M->IsSimulatingPhysics()) ? 1 : 0, (M && M->RigidBodyIsAwake()) ? 1 : 0);

		// Client VR tileset streaming / culling state.
		if (ClientVRTileset)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[BasketA][CLIENT] Tileset=%s | FogCulling=%d FrustumCulling=%d | LoadProgress=%.1f%%"),
				*GetNameSafe(ClientVRTileset),
				ClientVRTileset->EnableFogCulling ? 1 : 0,
				ClientVRTileset->EnableFrustumCulling ? 1 : 0,
				ClientVRTileset->GetLoadProgress());
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[BasketA][CLIENT] ClientVRTileset is NULL (not resolved by DEFAULT_TILESET tag)."));
		}
	}

	// ---- [Collision][SERVER] resolution + streaming probe (~1 Hz while authoritative) ----
	// Directly answers the three sink/hover questions at the rover's LIVE position:
	//   Q "tiles loaded here?"      -> central down-probe HIT (cooked tile present) vs MISS (nothing).
	//   Q "how coarse here?"        -> hit tile's Bounds.SphereRadius (deeper LOD = smaller tiles) plus
	//                                  a lateral facet scan: how far we can step before the surface
	//                                  normal changes. Big radius / big facet distance => coarse collision.
	//   Q "is the proxy cam working?"-> if the camera is registered ([CamMgr][SERVER]) AND feeding the
	//                                  tileset, the tile under the rover should be fine (small); a coarse
	//                                  tile right under a registered camera points at MaxSSE / consumption.
	// Footprint fan shows whether collision spans the whole rover, not just one point.
	if (HasAuthority() && GeoRefsManager && GeoRefsManager->GetVRMoon())
	{
		CollisionLogAccumulator += DeltaTime;
		if (CollisionLogAccumulator >= 1.0f)
		{
			CollisionLogAccumulator = 0.0f;

			UWorld* W               = GetWorld();
			const FVector RoverPos  = GetActorLocation();
			const FVector Center    = GeoRefsManager->GetVRMoon()->GetActorLocation();
			const FVector Down      = (Center - RoverPos).GetSafeNormal();
			const float   ProbeLen  = 5000000.0f;

			FCollisionQueryParams P(FName(TEXT("CollisionResProbe")), /*bTraceComplex=*/true, this);

			// Central probe: loaded? which tile? how coarse (tile bounds radius)? which tileset + its LOD settings?
			FHitResult C;
			const bool bC = W->LineTraceSingleByChannel(C, RoverPos, RoverPos + Down * ProbeLen, ECC_WorldStatic, P);
			if (bC)
			{
				ACesium3DTileset* TS = Cast<ACesium3DTileset>(C.GetActor());
				const float BoundsR  = C.GetComponent() ? C.GetComponent()->Bounds.SphereRadius : -1.f;
				UE_LOG(LogTemp, Warning,
					TEXT("[Collision][SERVER] Central HIT | clr=%.1f | tile=%s boundsR=%.1f | nrm=%s | tileset=%s load=%.1f%% MaxSSE=%.1f"),
					C.Distance, *GetNameSafe(C.GetComponent()), BoundsR, *C.ImpactNormal.ToString(),
					*GetNameSafe(C.GetActor()),
					TS ? TS->GetLoadProgress() : -1.f,
					TS ? TS->MaximumScreenSpaceError : -1.f);
			}
			else
			{
				UE_LOG(LogTemp, Error,
					TEXT("[Collision][SERVER] Central MISS | no cooked collision under rover (tile not loaded / no physics mesh here)."));
			}

			// Tangent basis for lateral probes.
			FVector T1 = FVector::CrossProduct(Down, FVector::UpVector);
			if (T1.IsNearlyZero()) { T1 = FVector::CrossProduct(Down, FVector::ForwardVector); }
			T1.Normalize();
			const FVector T2 = FVector::CrossProduct(Down, T1).GetSafeNormal();

			// Facet scan: smallest lateral offset at which the surface normal changes (coarse => far).
			// Negative result = we ran off loaded collision at that radius (a coverage gap / streaming edge).
			float FacetScaleCm = 0.f;
			if (bC)
			{
				static const float Offsets[] = { 10.f, 25.f, 50.f, 100.f, 200.f, 400.f, 800.f };
				for (float Off : Offsets)
				{
					const FVector Start = RoverPos + T1 * Off;
					FHitResult H;
					if (W->LineTraceSingleByChannel(H, Start, Start + Down * ProbeLen, ECC_WorldStatic, P))
					{
						if (FVector::DotProduct(H.ImpactNormal, C.ImpactNormal) < 0.999f) { FacetScaleCm = Off; break; }
					}
					else { FacetScaleCm = -Off; break; }
				}
			}

			// Footprint fan: coverage + clearance spread across the rover footprint.
			int32 FanHits = 0; const int32 FanN = 8;
			float MinClr = MAX_flt, MaxClr = -MAX_flt;
			for (int32 i = 0; i < FanN; ++i)
			{
				const float Ang     = (2.f * PI * i) / FanN;
				const FVector Dir   = (T1 * FMath::Cos(Ang) + T2 * FMath::Sin(Ang));
				const FVector Start = RoverPos + Dir * 150.f;
				FHitResult H;
				if (W->LineTraceSingleByChannel(H, Start, Start + Down * ProbeLen, ECC_WorldStatic, P))
				{
					++FanHits;
					MinClr = FMath::Min(MinClr, (float)H.Distance);
					MaxClr = FMath::Max(MaxClr, (float)H.Distance);
				}
			}
			const float Spread = (FanHits > 0) ? (MaxClr - MinClr) : -1.f;
			UE_LOG(LogTemp, Warning,
				TEXT("[Collision][SERVER] Fan hits=%d/%d clrSpread=%.1f | FacetScan(cm; first normal change, neg=coverage gap)=%.0f | LinVel=%.1f"),
				FanHits, FanN, Spread, FacetScaleCm, GetVelocity().Size());
		}
	}

	if (!PuppetRover || !GeoRefsManager)
	{
		return;
	}

	// Run transform of puppet on client
	if (!HasAuthority())
	{
		/*
		// Position
		const FVector MasterWorldPos = GetActorLocation();
		const FVector MasterLocalPos_VRMoon_UE = GeoRefsManager->GetVRMoon()->GetTransform().InverseTransformPosition(MasterWorldPos);
		PuppetRover->SetActorRelativeLocation(MasterLocalPos_VRMoon_UE);

		// Orientation
		const FQuat MasterOrientation_World = GetActorQuat();
		const FQuat MasterOrientation_Local = GeoRefsManager->GetVRMoon()->GetActorQuat().Inverse() * MasterOrientation_World;
		PuppetRover->SetActorRelativeRotation(MasterOrientation_Local);
		*/
		
		// Position: map the master's geodetic location (VR moon) onto the AR moon.
		const FVector MasterWorld         = GetActorLocation();
		const FVector MasterGeoPosition   = GeoRefsManager->UECoordsToVRMoonCoords(MasterWorld);   // world -> VR-moon LLH
		const FVector PuppetWorldPosition = GeoRefsManager->ARMoonCoordsToUECoords(MasterGeoPosition); // LLH -> AR-moon world

		// Orientation: transfer the master's pose THROUGH the local surface frame (N/E/U) at
		// its lat/long, mirroring how position is transferred through geodetic coords.
		auto SurfaceQuatWorld = [](ACesiumGeoreference* Geo, const FVector& GeoPos) -> FQuat
		{
			const FMatrix LocalBasis = UGeoUtils::GetLocalSpatialReferenceFrame(GeoPos, Geo);
			return Geo->GetActorQuat() * LocalBasis.ToQuat();
		};

		const FQuat MasterQuat = GetActorQuat();
		const FQuat VRSurface  = SurfaceQuatWorld(GeoRefsManager->GetVRMoon(), MasterGeoPosition);
		const FQuat ARSurface  = SurfaceQuatWorld(GeoRefsManager->GetARMoon(), MasterGeoPosition);

		const FQuat MasterPoseRelToSurface = VRSurface.Inverse() * MasterQuat;          // heading/tilt vs the ground
		const FQuat PuppetWorldOrientation = ARSurface * MasterPoseRelToSurface;        // same ground-relative pose on AR moon

		// ---- NaN guard + per-stage diagnostic ----
		// Pinpoints exactly which conversion stage first produces non-finite values, and refuses to
		// push NaN into the puppet (which would make it vanish and corrupt its replicated movement).
		const bool bMasterWorldOK = !MasterWorld.ContainsNaN();
		const bool bGeoOK         = !MasterGeoPosition.ContainsNaN();
		const bool bPuppetPosOK   = !PuppetWorldPosition.ContainsNaN();
		const bool bRotOK         = !PuppetWorldOrientation.ContainsNaN();

		if (bMasterWorldOK && bGeoOK && bPuppetPosOK && bRotOK)
		{
			// The master itself is already smoothed (eased toward the replicated target earlier this
			// Tick), so the puppet just copies it — one interpolation, both rovers smooth.
			PuppetRover->SetActorLocationAndRotation(PuppetWorldPosition, PuppetWorldOrientation);
		}

		if (bLogThisFrame || !bMasterWorldOK || !bGeoOK || !bPuppetPosOK || !bRotOK)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[Puppet][CLIENT] conv | MasterWorld=%s(ok%d) -> GeoLLH=%s(ok%d) -> PuppetWorld=%s(ok%d) | VRSurfNaN=%d ARSurfNaN=%d RotNaN=%d | applied=%d"),
				*MasterWorld.ToString(), bMasterWorldOK ? 1 : 0,
				*MasterGeoPosition.ToString(), bGeoOK ? 1 : 0,
				*PuppetWorldPosition.ToString(), bPuppetPosOK ? 1 : 0,
				VRSurface.ContainsNaN() ? 1 : 0, ARSurface.ContainsNaN() ? 1 : 0, PuppetWorldOrientation.ContainsNaN() ? 1 : 0,
				(bMasterWorldOK && bGeoOK && bPuppetPosOK && bRotOK) ? 1 : 0);
		}

		// ---- Geodetic round-trip verification ----
		// Convert the master's WORLD pos back to VR-moon geodetic, and the puppet's WORLD pos back
		// to AR-moon geodetic. Because the puppet was placed at the master's geodetic position on
		// the AR moon, these two LLH values MUST be identical. Any delta = a conversion bug.
		if (bLogThisFrame)
		{
			const FVector MasterGeo_VR = GeoRefsManager->UECoordsToVRMoonCoords(GetActorLocation());
			const FVector PuppetGeo_AR = GeoRefsManager->UECoordsToARMoonCoords(PuppetRover->GetActorLocation());
			UE_LOG(LogTemp, Warning,
				TEXT("[Puppet][CLIENT] GeoCheck | MasterGeo(VR)=%s | PuppetGeo(AR)=%s | delta=%s"),
				*MasterGeo_VR.ToString(), *PuppetGeo_AR.ToString(), *(PuppetGeo_AR - MasterGeo_VR).ToString());
		}
	}

	if (bLogThisFrame)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Puppet][%s] Tick: MasterWorld=%s | RelLoc(VRlocal)=%s | PuppetWorld=%s"),
			Net, *GetActorLocation().ToString(), *GeoRefsManager->GetVRMoon()->GetTransform().InverseTransformPosition(GetActorLocation()).ToString(), *PuppetRover->GetActorLocation().ToString());
	}
}

void AMasterRover::OnRep_ReplicatedMovement()
{
	Super::OnRep_ReplicatedMovement();

	// [BasketA] Record when/where the server's authoritative transform correction lands on the
	// client, for the RepDiv diagnostic (age = time since last correction; drift = local vs server).
	if (const UWorld* W = GetWorld())
	{
		LastRepMoveWorldTime = W->GetTimeSeconds();
	}
	++RepMoveUpdateCount;

	// [BasketA FIX] The client master has physics simulation disabled (see Tick), so Unreal's
	// physics-replication path (bRepPhysics=true, because the SERVER body simulates) no longer
	// applies the incoming transform. Instead of snapping the actor here (choppy at network
	// cadence), record it as the target that Tick eases the master toward, so the master moves
	// smoothly on the VR moon and the puppet copies that smoothed motion.
	const FRepMovement& RM = GetReplicatedMovement();
	LastRepMoveLocation = RM.Location;
	LastRepMoveRotation = RM.Rotation;
	bHasRepTarget = true;
}

FVector AMasterRover::GetLocalPos_UE() const
{
	const FVector WorldPos_UE = this->GetActorLocation();
	const FVector LocalPos_UE = GeoRefsManager->GetVRMoon()->GetTransform().InverseTransformPositionNoScale(WorldPos_UE);
	
	return LocalPos_UE;
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
	UE_LOG(LogTemp, Log, TEXT("MasterRover: Handling new controls..."))
	
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
