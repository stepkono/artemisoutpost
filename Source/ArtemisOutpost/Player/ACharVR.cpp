// Fill out your copyright notice in the Description page of Project Settings.


#include "ACharVR.h"
#include "Cesium3DTileset.h"
#include "EngineUtils.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "ArtemisOutpost/Miscellaneous/NetUtils.h"


// Sets default values
ACharVR::ACharVR()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	bAlwaysRelevant = true;

	// In VR the head orientation comes from the HMD (via the camera), NOT from the controller.
	// The capsule must never inherit controller pitch/roll/yaw or it would tilt the whole rig.
	// Yaw turning (thumbstick snap/smooth turn) should rotate the capsule explicitly, not via this.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw   = false;
	bUseControllerRotationRoll  = false;
}

// Called when the game starts or when spawned
void ACharVR::BeginPlay()
{
	Super::BeginPlay();

	// Resolve the VR rig and apply the standing/floor-level setup. Safe to call on every
	// instance: it no-ops unless this is the locally controlled pawn with an active HMD.
	InitVRComponents();

	// VR-moon tileset caching is client-only (used to show/hide the player's view).
	// The server host doesn't render and must not depend on it.
	if (ArtemisNet::IsServerHost(GetNetMode()))
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("ACharVR: Failed to get world."));
		return;
	}

	for (const auto TileSet : TActorRange<ACesium3DTileset>(World))
	{
		if (TileSet->ActorHasTag(FName("DEFAULT_TILESET")))
		{
			VRTileSet = TileSet;
			break; 
		}
	}
	if (!VRTileSet)
	{
		UE_LOG(LogTemp, Error, TEXT("ACharVR: Failed to initialize VR Moon tileset.")); 
	}
}

// Called every frame
void ACharVR::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// ---- VR head/roof collision ----
	// Runs for the local player's own HMD only (each client owns its head pose). Must run
	// regardless of net role, so it sits before the client-only probe's early return below.
	if (IsLocallyControlled() && UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled())
	{
		// One-time recenter: snap the HMD tracking origin onto this pawn so the camera sits on
		// the character regardless of where the player physically stands in their play space.
		if (!bHasRecenteredHMD)
		{
			bHasRecenteredHMD = true;
			UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition(0.f);

			// Confirm we really are the local view target (rules out a possession problem).
			if (const APlayerController* PC = Cast<APlayerController>(GetController()))
			{
				UE_LOG(LogTemp, Warning, TEXT("[VRChar] Recentered. Controller=%s ViewTarget=%s (this=%s)"),
					*GetNameSafe(PC), *GetNameSafe(PC->GetViewTarget()), *GetName());
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[VRChar] LocallyControlled but no PlayerController — NOT possessed locally."));
			}
		}

		UpdateHeadCollision(DeltaTime);
	}

	// ---- Client-side VR-moon collision probe ----
	// The VR character stands on the VR moon and the client has a real player camera
	// driving Cesium streaming, so this tells us whether the VR moon has collision on
	// the client. Compared with the server-side rover GroundProbe (same VR moon):
	//   client HIT + server MISS  -> server-only issue (no camera/streaming on the server)
	//   client MISS + server MISS -> general problem (Create Physics Meshes / channel / LOD)
	// Client-only: must not run on the listen-server host (its char is locally controlled too).
	if (!(IsLocallyControlled() && ArtemisNet::IsClientContext(GetNetMode())))
	{
		return;
	}

	DebugProbeAccumulator += DeltaTime;
	if (DebugProbeAccumulator < 5.0f)
	{
		return;
	}
	DebugProbeAccumulator = 0.0f;

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	
	if (!GeoRefsManager)
	{
		return; 
	}

	const FVector CharPos = GetActorLocation();
	const FVector Center  = GeoRefsManager->GetVRMoon()->GetActorLocation();          // VR moon centre (planet centre)
	const FVector Down    = (Center - CharPos).GetSafeNormal(); // toward moon centre = "down" on a globe
	const FVector End     = CharPos + Down * 500000000.0f;      // 5e8 UE units, long enough to cross the moon

	FHitResult Hit;
	FCollisionQueryParams Params(FName(TEXT("VRCharVRMoonProbe")), /*bTraceComplex=*/true, this);
	const bool bHit = World->LineTraceSingleByChannel(Hit, CharPos, End, ECC_WorldStatic, Params);

	if (bHit)
	{
		UE_LOG(LogTemp, Warning, TEXT("[VRChar][CLIENT] VRMoonProbe HIT: Actor=%s Comp=%s Dist=%.1f | CharPos=%s"),
			*GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()), Hit.Distance, *CharPos.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[VRChar][CLIENT] VRMoonProbe MISS | CharPos=%s | VRGeo=%s"),
			*CharPos.ToString(), *GeoRefsManager->GetVRMoon()->GetName());
	}
}

// Called to bind functionality to input
void ACharVR::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void ACharVR::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();
	
	UE_LOG(LogTemp, Log, TEXT("ACharVR: NotifyControllerChanged"));
	
	
	// Check locally 
	if (GetLocalRole() == ROLE_AutonomousProxy)
	{
		const AController* CurrentController = GetController();
		
		if (!VRTileSet)
		{
			UE_LOG(LogTemp, Error, TEXT("ACharVR: Tileset it not set."))
			return; 
		}
		
		// UNPOSSESSED
		if (CurrentController == nullptr)
		{
			UE_LOG(LogTemp, Log, TEXT("ACharVR: UNPOSSESSED"));
			//VRTileSet->SetActorHiddenInGame(true);
		}
		//POSSESSED
		else
		{
			UE_LOG(LogTemp, Log, TEXT("ACharVR: POSSESSED"));
			//VRTileSet->SetActorHiddenInGame(false);
		}		
	}
}

void ACharVR::SetBase(UPrimitiveComponent* NewBase, FName BoneName, bool bNotifyActor)
{
	// Cesium tiles are streamed at runtime and are not network-addressable, so they cannot be
	// replicated as a movement base. Refuse them so the character replicates absolute position
	// instead of base-relative — see header comment for the full rationale.
	//
	// We can't reference the tile component type directly: UCesiumGltfPrimitiveComponent lives in
	// CesiumRuntime's Private folder and isn't exported. Instead we identify it by its OWNER — the
	// tile meshes are always owned by the ACesium3DTileset actor, which is a public type.
	if (NewBase && NewBase->GetOwner() && NewBase->GetOwner()->IsA<ACesium3DTileset>())
	{
		NewBase = nullptr;
	}

	Super::SetBase(NewBase, BoneName, bNotifyActor);
}

ACesium3DTileset* ACharVR::GetVRTileset()
{
	return VRTileSet;
}

void ACharVR::SetGeoRefsManager(AGeoRefsManager* InManager)
{
	GeoRefsManager = InManager;
}

void ACharVR::InitVRComponents()
{
	// Find the rig components authored on BP_VRChar by their tags. Done by tag (rather than
	// CreateDefaultSubobject in C++) because the components live in the Blueprint child.
	TInlineComponentArray<USceneComponent*> SceneComps(this);
	for (USceneComponent* Comp : SceneComps)
	{
		if (!Comp)
		{
			continue;
		}
		if (!CachedVROrigin && Comp->ComponentHasTag(VROriginTag))
		{
			CachedVROrigin = Comp;
		}
		if (!CachedVRCamera && Comp->ComponentHasTag(VRCameraTag))
		{
			CachedVRCamera = Cast<UCameraComponent>(Comp);
		}
	}

	if (!CachedVROrigin)
	{
		UE_LOG(LogTemp, Error, TEXT("ACharVR: No component tagged '%s' (VR origin) found on %s."),
			*VROriginTag.ToString(), *GetName());
		return;
	}
	if (!CachedVRCamera)
	{
		UE_LOG(LogTemp, Error, TEXT("ACharVR: No UCameraComponent tagged '%s' (VR camera) found on %s."),
			*VRCameraTag.ToString(), *GetName());
	}

	// The HMD drives the camera transform directly (true by default, set explicitly for clarity).
	if (CachedVRCamera)
	{
		CachedVRCamera->bLockToHmd = true;
	}

	// Place the tracking origin at the bottom of the capsule (the player's feet). Combined with
	// a floor-level HMD tracking origin below, this puts the camera at the player's real-world
	// head height and makes physical crouching/squatting "just work": the HMD lowers, so does
	// the camera, with no extra code.
	if (const UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		VROriginBaseZ = -Capsule->GetScaledCapsuleHalfHeight();
	}

	// Only the local player with a live HMD should be re-homed to floor level and have its
	// tracking origin changed. Remote/simulated proxies keep the Blueprint-authored layout.
	if (!(IsLocallyControlled() && UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled()))
	{
		return;
	}

	CachedVROrigin->SetRelativeLocation(FVector(0.0f, 0.0f, VROriginBaseZ));

	// Floor-level tracking: HMD pose is reported relative to the physical floor, so the player
	// can walk/lean/crouch within their play space and the camera mirrors it 1:1.
	UHeadMountedDisplayFunctionLibrary::SetTrackingOrigin(EHMDTrackingOrigin::LocalFloor);
}

void ACharVR::UpdateHeadCollision(float DeltaTime)
{
	if (!CachedVROrigin)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// VROrigin is a child of the capsule (the root), so the actor transform is its parent.
	// "Base" = the uncorrected origin at the feet; we always trace from this fixed reference
	// so the result can't oscillate with last frame's applied offset.
	const FTransform ActorXform = GetActorTransform();
	const FVector BaseWorldLoc  = ActorXform.TransformPosition(FVector(0.0f, 0.0f, VROriginBaseZ));
	const FQuat   OriginRot     = ActorXform.GetRotation();          // VROrigin carries no relative rotation
	const FVector UpDir         = ActorXform.GetUnitAxis(EAxis::Z);  // capsule "up" (gravity-aligned on the globe)

	// Where the HMD currently wants the head, relative to the (uncorrected) tracking origin.
	FRotator HmdRot;
	FVector  HmdPos;
	UHeadMountedDisplayFunctionLibrary::GetOrientationAndPosition(HmdRot, HmdPos);
	const FVector HeadWorldLoc = BaseWorldLoc + OriginRot.RotateVector(HmdPos);

	float DesiredPush = 0.0f;
	if (bEnableHeadCollision)
	{
		// Sweep a sphere from just above the feet up to the head. A hit means the head would
		// be inside/through geometry (e.g. a low roof); push the rig down by the penetration
		// along "up" so the camera ends up resting just below the obstruction.
		const FVector TraceStart = BaseWorldLoc + UpDir * HeadCollisionRadius;

		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(VRHeadCollision), /*bTraceComplex=*/false, this);
		if (World->SweepSingleByChannel(Hit, TraceStart, HeadWorldLoc, FQuat::Identity,
			ECC_WorldStatic, FCollisionShape::MakeSphere(HeadCollisionRadius), Params))
		{
			DesiredPush = FMath::Max(0.0f, FVector::DotProduct(HeadWorldLoc - Hit.Location, UpDir));
		}
	}

	// Smooth the offset so the view eases under obstructions rather than snapping (comfort).
	CurrentHeadCollisionPush = FMath::FInterpTo(CurrentHeadCollisionPush, DesiredPush, DeltaTime, HeadCollisionInterpSpeed);
	CachedVROrigin->SetRelativeLocation(FVector(0.0f, 0.0f, VROriginBaseZ - CurrentHeadCollisionPush));
}
