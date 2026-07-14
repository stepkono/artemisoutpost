// Fill out your copyright notice in the Description page of Project Settings.


#include "ACharVR.h"
#include "Cesium3DTileset.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "IXRTrackingSystem.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "ArtemisOutpost/Networking/ClientServerConnection/NetUtils.h"


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

	// Resolve/cache the VR rig (safe on every instance), then attempt the local floor-level setup.
	// On a networked client possession usually hasn't happened yet at BeginPlay, so this attempt
	// will no-op and NotifyControllerChanged will apply it once we actually become locally controlled.
	InitVRComponents();
	ApplyLocalVRSetup();

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
		UpdateVRViewTilt();
		UpdateHeadCollision(DeltaTime);
	}

	// TEMP diagnostic: only for our own pawn, throttled to ~1 Hz.
	if (IsLocallyControlled())
	{
		LogVRTransforms(DeltaTime);
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

	// Possession on the owning client arrives here (after BeginPlay), which is the first point at
	// which IsLocallyControlled() is reliably true. Apply the deferred local VR setup now.
	ApplyLocalVRSetup();

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

	// NOTE: The VR stereo view is rendered by the XR system from a tracking space whose ROTATION is
	// forced to world/gravity-up; it uses the rig POSITION but ignores the camera component's
	// orientation. Neither bLockToHmd nor manually posing the camera can tilt the rendered horizon
	// to the moon surface normal (verified via LogVRTransforms). The only levers are (a) rotating the
	// XR tracking space via base orientation, or (b) aligning the VR play area to world-up. Left at
	// the default lock; the actual horizon fix lives elsewhere.
	if (CachedVRCamera)
	{
		CachedVRCamera->bLockToHmd = true;
	}

	// Compute the feet-level origin offset once. Combined with a floor-level HMD tracking origin
	// (applied in ApplyLocalVRSetup), this puts the camera at the player's real-world head height
	// and makes physical crouching/squatting "just work": the HMD lowers, so does the camera.
	if (const UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		VROriginBaseZ = -Capsule->GetScaledCapsuleHalfHeight();
	}
}

void ACharVR::ApplyLocalVRSetup()
{
	// Idempotent: only the local player with a live HMD gets re-homed to floor level and has the
	// (global) tracking origin changed. Remote/simulated proxies keep the Blueprint-authored layout.
	// In networked play the owning client possesses AFTER BeginPlay, so this is also invoked from
	// NotifyControllerChanged — the one-shot flag makes whichever call wins the race the only one
	// that takes effect.
	if (bLocalVRSetupApplied || !CachedVROrigin)
	{
		return;
	}
	if (!(IsLocallyControlled() && UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled()))
	{
		return;
	}

	CachedVROrigin->SetRelativeLocation(FVector(0.0f, 0.0f, VROriginBaseZ));

	// Floor-level tracking: HMD pose is reported relative to the physical floor, so the player
	// can walk/lean/crouch within their play space and the camera mirrors it 1:1.
	UHeadMountedDisplayFunctionLibrary::SetTrackingOrigin(EHMDTrackingOrigin::LocalFloor);

	bLocalVRSetupApplied = true;
	UE_LOG(LogTemp, Log, TEXT("ACharVR: Local VR setup applied (VROrigin re-homed to feet, LocalFloor tracking) on %s."), *GetName());
}

void ACharVR::UpdateVRViewTilt()
{
	if (!bAlignVRViewToSurface)
	{
		return;
	}

	// Base-orientation lives on the XR tracking system, not the HMD function library.
	if (!GEngine || !GEngine->XRSystem.IsValid())
	{
		return;
	}

	// The pawn is rotated (in BP) so its up axis = the moon surface normal. We want the rendered VR
	// horizon to match that. The compositor renders the head pose as TrackingToWorld(rotation forced
	// to world-up) * HMDPose, so the only rotation we can influence is the tracking-space base.
	//
	// TiltQ is the minimal rotation that takes world-up (Z) to the surface normal. Applying it as the
	// base orientation rotates the whole tracking space, so a physically level head renders looking
	// at the local (tilted) horizon. Using FindBetweenNormals (up-only) rather than the full pawn
	// rotation avoids forcing the pawn's yaw onto the view, so head yaw stays HMD-controlled.
	const FVector SurfaceUp = GetActorUpVector();
	FQuat TiltQ = FQuat::FindBetweenNormals(FVector::UpVector, SurfaceUp);

	// Base-orientation sign convention differs between runtimes; bInvertVRViewTilt lets us settle it
	// live in one PIE session (watch the [VRXform] VRCamera WorldUp line vs the surface normal).
	if (bInvertVRViewTilt)
	{
		TiltQ = TiltQ.Inverse();
	}

	GEngine->XRSystem->SetBaseOrientation(TiltQ);
}

void ACharVR::LogVRTransforms(float DeltaTime)
{
	// Throttle to ~1 Hz so the log stays readable.
	VRDebugLogTimer += DeltaTime;
	if (VRDebugLogTimer < 1.0f)
	{
		return;
	}
	VRDebugLogTimer = 0.0f;

	auto V = [](const FVector& X) { return FString::Printf(TEXT("(%.3f, %.3f, %.3f)"), X.X, X.Y, X.Z); };
	auto R = [](const FRotator& X) { return FString::Printf(TEXT("P=%.1f Y=%.1f R=%.1f"), X.Pitch, X.Yaw, X.Roll); };

	// 1) The actor (capsule root). If the rig were rotated to the surface normal, ActorUp would be
	//    the diagonal moon-up vector — NOT (0,0,1). If it prints (0,0,1) the actor is NOT rotated.
	UE_LOG(LogTemp, Warning, TEXT("[VRXform] ActorRot[%s]  ActorUp=%s"),
		*R(GetActorRotation()), *V(GetActorUpVector()));

	// 2) VROrigin — should inherit the actor rotation (its Up should match ActorUp).
	if (CachedVROrigin)
	{
		UE_LOG(LogTemp, Warning, TEXT("[VRXform] VROrigin  WorldRot[%s]  WorldUp=%s  RelLoc=%s"),
			*R(CachedVROrigin->GetComponentRotation()),
			*V(CachedVROrigin->GetUpVector()),
			*V(CachedVROrigin->GetRelativeLocation()));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[VRXform] VROrigin = NULL (tag not resolved)"));
	}

	// 3) VRCamera — this is what you actually see through. If its WorldUp stays ~(0,0,1) while the
	//    actor/VROrigin Up is the diagonal moon-up, then the HMD view is NOT inheriting the tilt
	//    (the real bug). If its WorldUp matches ActorUp, orientation is being inherited correctly.
	if (CachedVRCamera)
	{
		UE_LOG(LogTemp, Warning, TEXT("[VRXform] VRCamera  WorldRot[%s]  WorldUp=%s  RelRot[%s]  LockToHmd=%d"),
			*R(CachedVRCamera->GetComponentRotation()),
			*V(CachedVRCamera->GetUpVector()),
			*R(CachedVRCamera->GetRelativeRotation()),
			CachedVRCamera->bLockToHmd ? 1 : 0);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[VRXform] VRCamera = NULL (tag not resolved)"));
	}

	// 4) Raw HMD pose (tracking space). This is what the runtime reports before the parent transform
	//    is applied — HmdRot is relative to the tracking origin, so its Up is real-world up.
	if (UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled())
	{
		FRotator HmdRot;
		FVector  HmdPos;
		UHeadMountedDisplayFunctionLibrary::GetOrientationAndPosition(HmdRot, HmdPos);
		UE_LOG(LogTemp, Warning, TEXT("[VRXform] HMD raw   Rot[%s]  Pos=%s  LocalSetupApplied=%d"),
			*R(HmdRot), *V(HmdPos), bLocalVRSetupApplied ? 1 : 0);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[VRXform] HMD not enabled  LocalSetupApplied=%d"), bLocalVRSetupApplied ? 1 : 0);
	}
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
