// Fill out your copyright notice in the Description page of Project Settings.


#include "ACharVR.h"
#include "Cesium3DTileset.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "IXRTrackingSystem.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Blueprint/UserWidget.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "ArtemisOutpost/XR/XRUtilsSubsystem.h"
#include "ArtemisOutpost/Networking/ClientServerConnection/NetUtils.h"
#include "ArtemisOutpost/Player/PlayerController/PawnController.h"
#include "EnhancedInputComponent.h"
#include "ArtemisOutpost/Moon/Cesium/GeoTools/GeoUtils.h"
#include "ToolsHUD/ToolsHUDComponent.h"
#include "ControllerRays/ControllerRayComponent.h"
#include "ArtemisOutpost/Player/PlayerCues/PlayerCuesManager.h"
#include "ArtemisOutpost/Player/PlayerCues/AwarenessHUD/AwarenessHUDComponent.h"
#include "ArtemisOutpost/Player/PlayerCues/VRCues/VRCuesPresenterComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/CharacterMovementComponent.h"


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

	// Client-local wrist Tools-HUD. It's a plain ActorComponent (no scene transform); the widget it
	// hosts renders into a WidgetComponent authored under the right controller in BP_VRChar.
	ToolsHUDComponent = CreateDefaultSubobject<UToolsHUDComponent>(TEXT("ToolsHUDComponent"));

	// Client-local controller rays (both hands). A plain ActorComponent; it resolves the
	// WidgetInteractionComponents authored in BP_VRChar and spawns the Niagara visuals at runtime.
	ControllerRayComponent = CreateDefaultSubobject<UControllerRayComponent>(TEXT("ControllerRayComponent"));

	// Awareness cues: this pawn's hits are expressed against the VR moon.
	PlayerCuesManager = CreateDefaultSubobject<UPlayerCuesManager>(TEXT("PlayerCuesManager"));
	PlayerCuesManager->SetUsesARMoon(false);

	AwarenessHUDComponent = CreateDefaultSubobject<UAwarenessHUDComponent>(TEXT("AwarenessHUDComponent"));

	this->Tags.AddUnique(FName("Blocking"));
}

void ACharVR::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(ACharVR, GeoRefsManager);
}

// Called when the game starts or when spawned
void ACharVR::BeginPlay()
{
	Super::BeginPlay();
	
	//SetIsPossessed(true); //TODO: TestLvl Only, remove in game 

	// Resolve/cache the VR rig (safe on every instance), then attempt the local floor-level setup.
	// On a networked client possession usually hasn't happened yet at BeginPlay, so this attempt
	// will no-op and NotifyControllerChanged will apply it once we actually become locally controlled.
	InitComponentsFromBP();
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

	// TEMP diagnostic — runs on EVERY instance, in both possessed/unpossessed states and even
	// before local setup, so we can see the real state during the "outside the body" repro
	// (which system is moving the rig, and where the capsule/origin/camera actually end up).
	LogVRTransforms(DeltaTime);

	// Must run before the bLocalVRSetupApplied gate below: the puppets other players see are driven
	// by the REMOTE proxies of this pawn, which never pass that gate.
	UpdateARPuppetTransform();

	// Only the pawn that set up the local VR view manages the GLOBAL XR base orientation.
	// bLocalVRSetupApplied is true ONLY for our own HMD pawn (set in ApplyLocalVRSetup) and stays
	// true across possess/unpossess. This is what keeps a remote player's proxy ACharVR — which also
	// ticks on this machine, with bIsPossessed=false — from calling ResetXRBaseOrientation and
	// flattening OUR horizon. It also works whether or not our pawn is still engine-possessed in AR
	// (IsLocallyControlled() would go false there and be indistinguishable from a remote proxy).
	if (!bLocalVRSetupApplied)
	{
		return;
	}
	
	// The tilt may only run while THIS pawn renders the local view. bIsPossessed is a Blueprint flag
	// (and is forced on in BeginPlay for TestLvl), so it can stay true after the switch back to AR.
	// If UpdateVRViewTilt keeps running then, the base orientation rotates the whole tracking space
	// (HMD and spatial anchors alike, MetaXR folds it into every pose) by the parked VR character's
	// surface normal every frame: the anchors leave the physical table plane, the AR moon is re-seeded
	// along a stale table normal and drifts or disappears. Engine possession is the reliable signal:
	// in VR this pawn is locally controlled, in AR the AR pawn is. Remote proxies never reach this
	// point (bLocalVRSetupApplied gate above).
	// The VR character stays engine-possessed after the first switch to VR (verified in the Quest
	// log), so IsLocallyControlled() alone cannot tell AR from VR. Ask the controller for the mode
	// when it is our APawnController. Without one (TestLvl) fall back to the Blueprint flag.
	bool bModeIsVR = true;
	if (const APawnController* PC = Cast<APawnController>(GetController()))
	{
		bModeIsVR = (PC->GetXRMode() == EXRMode::VR);
	}
	const bool bDrivesLocalView = bIsLocallyPossessed && IsLocallyControlled() && bModeIsVR;
	if (bIsLocallyPossessed && !bDrivesLocalView)
	{
		if (!bTiltSuppressedLogged)
		{
			UE_LOG(LogTemp, Warning, TEXT("ACharVR: %s has bIsPossessed=true but is not the VR view (locallyControlled=%d modeVR=%d). VR view tilt suppressed, base orientation reset."),
				*GetName(), IsLocallyControlled() ? 1 : 0, bModeIsVR ? 1 : 0);
			bTiltSuppressedLogged = true;
		}
	}
	else
	{
		bTiltSuppressedLogged = false;
	}

	// The VR cue visualizers exist exactly while this pawn renders the local VR view.
	UpdateVRCuesPresenter(bDrivesLocalView);

	if (bDrivesLocalView)
	{
		// VR mode: tilt the horizon to the surface and keep the body under the HMD.
		bXRBaseIsReset = false;
		UpdateVRViewTilt();
		UpdateCapsuleFollowsHMD();
		// LogVRMotionAnomalies(DeltaTime);
		
		// NOTE: UpdateHeadCollision is temporarily not called — it re-homes VROrigin's full relative
		// location every frame, which would fight UpdateCapsuleFollowsHMD's horizontal offset. Roof
		// avoidance needs to be folded into the capsule-follow step before re-enabling.
	}
	else if (!bXRBaseIsReset)
	{
		// Switched out of VR (AR): clear our tilt exactly once per transition.
		if (UGameInstance* GameInst = GetGameInstance())
		{
			if (UXRUtilsSubsystem* XRUtils = GameInst->GetSubsystem<UXRUtilsSubsystem>())
			{
				XRUtils->ResetXRBaseOrientation();
			}
		}
		bXRBaseIsReset = true;
	}
}

void ACharVR::UpdateARPuppetTransform()
{
	if (!ARPuppet || !GeoRefsManager)
	{
		return;
	}
	// ---- Drive the puppet (client) ----
	// Map the master's geodetic location on the VR moon onto the AR moon, and transfer its pose
	// through the local surface frame (N/E/U) at that lat/long
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
		if (!MasterWorld.ContainsNaN() && !MasterGeoPosition.ContainsNaN() && !PuppetWorldPosition.ContainsNaN() && !PuppetWorldOrientation.ContainsNaN())
		{
			ARPuppet->SetActorLocationAndRotation(PuppetWorldPosition, PuppetWorldOrientation);
		}
	}
}

// Called to bind functionality to input
void ACharVR::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// This only runs on the locally-controlled pawn (after possession), which is exactly when the HUD
	// input should bind. Locomotion is bound in BP_VRPawn; both coexist on the same EnhancedInput
	// component. The HUD's "focus" is handled purely via its own mapping context (added while open),
	// so no SetInputMode juggling is needed.
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (ToolsHUDComponent)
		{
			ToolsHUDComponent->BindInput(EIC);
		}
		if (ControllerRayComponent)
		{
			ControllerRayComponent->BindInput(EIC);
		}
		if (PlayerCuesManager)
		{
			PlayerCuesManager->BindInput(EIC);
		}
		if (AwarenessHUDComponent)
		{
			AwarenessHUDComponent->BindInput(EIC);
		}
	}
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
			//GetWorld()->GetGameInstance()->GetSubsystem<UXRUtilsSubsystem>()->ResetXRBaseOrientation();
			//VRTileSet->SetActorHiddenInGame(true);
		}
		//POSSESSED
		else
		{
			UE_LOG(LogTemp, Log, TEXT("ACharVR: POSSESSED"));
			UE_LOG(LogTemp, Log, TEXT("ACharVR [CLIENT]: ACharVR Character Position: %s"), *GetActorLocation().ToString());
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

void ACharVR::InitComponentsFromBP()
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
		if (!SkeletalMesh && Comp->ComponentHasTag(FName("Skeletal")))
		{
			SkeletalMesh = Cast<USkeletalMeshComponent>(Comp);
		}
		if (!CachedMinigameView && Comp->ComponentHasTag(MinigameViewTag))
		{
			CachedMinigameView = Cast<UWidgetComponent>(Comp);
		}
		if (!CachedMinigameDim && Comp->ComponentHasTag(MinigameDimTag))
		{
			CachedMinigameDim = Comp;
		}
	}

	// The minigame HUD + dimming start hidden; the controller reveals them on join.
	if (CachedMinigameView)
	{
		CachedMinigameView->SetVisibility(false, true);
	}
	if (CachedMinigameDim)
	{
		CachedMinigameDim->SetVisibility(false, true);
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
	if (!SkeletalMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("ACharVR: No USkeletalMeshComponent tagged '%s' found on %s"), *FString("Skeletal"), *GetName());
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

void ACharVR::ShowMinigameView(UUserWidget* Widget)
{
	// Last hop of the enter chain: the View exists, it now has to be visible in the HMD.
	if (CachedMinigameView)
	{
		CachedMinigameView->SetWidget(Widget);
		CachedMinigameView->SetVisibility(true, true);
		UE_LOG(LogMinigame, Log, TEXT("[View] %s (%s): %s placed in the world-space holder '%s', dim=%s. The minigame is now on screen for this player."),
			*GetName(), ArtemisNet::RoleName(GetNetMode()), *GetNameSafe(Widget), *MinigameViewTag.ToString(),
			CachedMinigameDim ? TEXT("yes") : TEXT("no holder found"));
	}
	else
	{
		UE_LOG(LogMinigame, Error, TEXT("[View] %s (%s): no UWidgetComponent tagged '%s' on this pawn -> the View %s was created but has nowhere to render. Check the tag on BP_VRChar."),
			*GetName(), ArtemisNet::RoleName(GetNetMode()), *MinigameViewTag.ToString(), *GetNameSafe(Widget));
	}

	if (CachedMinigameDim)
	{
		CachedMinigameDim->SetVisibility(true, true);
	}

	MiniGameIsOpen();
}

void ACharVR::HideMinigameView()
{
	if (CachedMinigameView)
	{
		CachedMinigameView->SetWidget(nullptr);
		CachedMinigameView->SetVisibility(false, true);
	}
	if (CachedMinigameDim)
	{
		CachedMinigameDim->SetVisibility(false, true);
	}
	
	MiniGameIsClosed();
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

	// Floor-relative rig: VROrigin at the feet, floor-level HMD tracking. The camera is therefore at
	// the player's real head height above the ground, and sitting down / standing up both read
	// naturally (the HMD height changes, the camera follows). The player's real height maps 1:1, so
	// the mannequin should be sized to roughly the player's standing height for the eyeline to match.
	VROriginBaseZ = -GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	CachedVROrigin->SetRelativeLocation(FVector(0.0f, 0.0f, VROriginBaseZ));

	// Keep the VR rig WORLD-ALIGNED in rotation. The capsule is tilted to the surface normal (for
	// the body mesh + gravity), and VROrigin would normally inherit that tilt. But the horizon fix
	// already tilts the whole tracking space via SetBaseOrientation — so an additionally-tilted
	// VROrigin double-rotates the HMD position offset and drops the camera below the feet
	// (measured: HeightAlongUp ≈ -86 instead of +120). Absolute rotation makes VROrigin ignore the
	// capsule tilt for ROTATION, while its relative LOCATION still follows the capsule (location
	// stays parent-relative). Net: base orientation applies the tilt exactly once, so the camera
	// lands at the intended height along the surface normal.
	CachedVROrigin->SetUsingAbsoluteRotation(true);
	CachedVROrigin->SetWorldRotation(FRotator::ZeroRotator);

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

FRotator ACharVR::GetTrackingSpaceTilt() const
{
	// The runtime applies the base orientation inverted (see bInvertVRViewTilt: reported = Base^-1 * device),
	// so the inverse of the base is the rotation actually applied to every tracked pose.
	if (GEngine && GEngine->XRSystem.IsValid())
	{
		return GEngine->XRSystem->GetBaseOrientation().Inverse().Rotator();
	}
	return FRotator::ZeroRotator;
}

FVector ACharVR::GetTrackingSpacePivot() const
{
	return UHeadMountedDisplayFunctionLibrary::GetTrackingToWorldTransform(const_cast<ACharVR*>(this)).GetLocation();
}

void ACharVR::UpdateVRCuesPresenter(bool bShouldPresent)
{
	if (bShouldPresent == bVRCuesPresenting)
	{
		return;
	}
	bVRCuesPresenting = bShouldPresent;

	UVRCuesPresenterComponent* Presenter = FindComponentByClass<UVRCuesPresenterComponent>();
	if (!Presenter)
	{
		UE_LOG(LogTemp, Warning, TEXT("ACharVR: %s has no UVRCuesPresenterComponent, VR cue visualizers stay off (add it in BP_VRChar)."), *GetName());
		return;
	}
	Presenter->SetPresenting(bShouldPresent);
}

void ACharVR::UpdateCapsuleFollowsHMD()
{
	if (!bLocalVRSetupApplied || !CachedVROrigin || !CachedVRCamera)
	{
		return;
	}

	// How far the HMD/camera has physically drifted from the capsule centre, measured in the surface
	// tangent plane (perpendicular to the moon-up axis). This is the horizontal "you walked away from
	// your body" component — vertical head movement (sit/stand/crouch) is intentionally left alone.
	const FVector Up          = GetActorUpVector();
	const FVector CamWorld    = CachedVRCamera->GetComponentLocation();
	const FVector Delta       = CamWorld - GetActorLocation();
	const FVector HorizOffset = Delta - FVector::DotProduct(Delta, Up) * Up;

	// Record the capsule-follow input for the motion diagnostics BEFORE the early-out, so a frame
	// with (correctly) no drift resets the captured values instead of leaving last frame's stale ones.
	LastHorizOffsetSize = HorizOffset.Size();
	if (HorizOffset.SizeSquared() < KINDA_SMALL_NUMBER)
	{
		LastCapsuleFollowMoved = 0.0f;
		bLastCapsuleFollowBlocked = false;
		return;
	}

	// Slide the capsule under the camera (sweep so the body collides with the world), then slide the
	// VR origin back by the same amount so the CAMERA does not move in the world — only the body
	// catches up. Net: you can never physically walk out of your capsule; walking moves the whole
	// character, and thumbstick locomotion (which moves the actor) carries the camera with it.
	//
	// NOTE: this is the minimal version. Known follow-ups if needed: (1) on a swept collision the
	// capsule moves less than HorizOffset, so counter-sliding by the full amount pushes the view back
	// slightly — for wall-stop comfort, counter-slide by the ACTUAL moved delta instead; (2) networked
	// room-scale movement / head-through-wall fade are handled robustly by VRExpansion if this proves
	// insufficient.
	// Capture how far the swept move ACTUALLY got (vs the requested HorizOffset) and whether it was
	// blocked. A follow that is repeatedly blocked while the counter-slide still applies the full
	// offset is a prime suspect for the intermittent runaway drift, so the diagnostics must see it.
	const FVector FollowBeforeLoc = GetActorLocation();
	FHitResult FollowHit;
	AddActorWorldOffset(HorizOffset,true, &FollowHit);
	LastCapsuleFollowMoved = (GetActorLocation() - FollowBeforeLoc).Size();
	bLastCapsuleFollowBlocked = FollowHit.bBlockingHit;
	CachedVROrigin->AddWorldOffset(-HorizOffset);
}

void ACharVR::LogVRMotionAnomalies(float DeltaTime)
{
	if (!CachedVRCamera)
	{
		return;
	}

	const FVector Up         = GetActorUpVector();
	const FVector ActorLoc   = GetActorLocation();
	const FVector CamWorld   = CachedVRCamera->GetComponentLocation();
	const float   HalfHeight = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.0f;
	const FVector Velocity   = GetVelocity();
	const float   Speed      = Velocity.Size();
	const float   VertSpeed  = FVector::DotProduct(Velocity, Up); // + = moving along the surface normal (up)

	// Raw HMD pose in tracking space. A doffed headset on a table can jump/reset this, which is what
	// we suspect drives the fly-up, so we watch its per-frame delta directly.
	FVector  HmdPos = FVector::ZeroVector;
	FRotator HmdRot = FRotator::ZeroRotator;
	if (UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled())
	{
		UHeadMountedDisplayFunctionLibrary::GetOrientationAndPosition(HmdRot, HmdPos);
	}

	// First valid frame: seed baselines and skip (frame deltas would be meaningless).
	if (!bMotionBaselineValid)
	{
		PrevActorLocation     = ActorLoc;
		PrevHmdPos            = HmdPos;
		PrevCameraWorld       = CamWorld;
		PrevCapsuleHalfHeight = HalfHeight;
		bMotionBaselineValid  = true;
		return;
	}

	const float ActorMove       = (ActorLoc - PrevActorLocation).Size();
	const float HmdMove         = (HmdPos - PrevHmdPos).Size();
	const float CamMove         = (CamWorld - PrevCameraWorld).Size();
	const float HalfHeightDelta = FMath::Abs(HalfHeight - PrevCapsuleHalfHeight);

	const UCharacterMovementComponent* CMC = GetCharacterMovement();
	const bool  bFalling  = CMC ? CMC->IsFalling() : false;
	const int32 MoveMode  = CMC ? static_cast<int32>(CMC->MovementMode.GetValue()) : -1; // 1=Walking 3=Falling 4=Flying

	// Floor + gravity-alignment state — the crux of the fall/fly-up. If FloorHit=0 while Falling, the
	// CMC has no ground under the capsule (Cesium collision missing/late, or the capsule is penetrating).
	// If FloorHit=1 but Walkable=0, the floor IS there but rejected (slope/perch). GravVsUpDeg should be
	// ~0 (custom gravity aligned to the pawn up); a large value means the CMC up-axis and the pawn up
	// disagree. FloorNvsUpDeg is the terrain slope the CMC sees relative to the pawn up.
	const bool  bOnGround      = CMC ? CMC->IsMovingOnGround() : false;
	const bool  bFloorHit      = CMC ? CMC->CurrentFloor.bBlockingHit : false;
	const bool  bFloorWalkable = CMC ? CMC->CurrentFloor.bWalkableFloor : false;
	const float FloorDist      = bFloorHit ? CMC->CurrentFloor.GetDistanceToFloor() : -1.0f;
	const FVector GravDir      = CMC ? CMC->GetGravityDirection() : FVector::DownVector;
	const float GravVsUpDeg    = CMC ? FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(-GravDir, Up), -1.0f, 1.0f))) : -1.0f;
	const float FloorNvsUpDeg  = bFloorHit ? FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(CMC->CurrentFloor.HitResult.ImpactNormal, Up), -1.0f, 1.0f))) : -1.0f;

	// Any threshold tripped this frame opens a short logging burst so we can watch the event evolve
	// over the following frames, not just the single spike.
	const bool bAnomaly =
		ActorMove       > MotionLogActorJumpCm       ||
		Speed           > MotionLogSpeedCmS          ||
		HalfHeightDelta > MotionLogHalfHeightDeltaCm ||
		HmdMove         > MotionLogHmdJumpCm         ||
		bFalling;

	if (bAnomaly)
	{
		MotionLogBurstFrames = FMath::Max(MotionLogBurstFrames, MotionLogBurstLength);
	}

	if (MotionLogBurstFrames > 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[VRMotion] dt=%.4f ActorMove=%.1f Speed=%.1f VertSpeed=%.1f Falling=%d MoveMode=%d | HalfH=%.1f dHalfH=%.1f | HMDmove=%.1f CamMove=%.1f | FollowOffset=%.1f FollowMoved=%.1f FollowBlocked=%d"),
			DeltaTime, ActorMove, Speed, VertSpeed, bFalling ? 1 : 0, MoveMode,
			HalfHeight, HalfHeightDelta, HmdMove, CamMove,
			LastHorizOffsetSize, LastCapsuleFollowMoved, bLastCapsuleFollowBlocked ? 1 : 0);
		UE_LOG(LogTemp, Warning,
			TEXT("[VRFloor]  OnGround=%d FloorHit=%d Walkable=%d FloorDist=%.1f | GravVsUpDeg=%.1f FloorNvsUpDeg=%.1f"),
			bOnGround ? 1 : 0, bFloorHit ? 1 : 0, bFloorWalkable ? 1 : 0, FloorDist, GravVsUpDeg, FloorNvsUpDeg);

		// Name the "invisible wall". When we have velocity, sweep the capsule a short way along it using
		// the capsule's OWN collision profile, so we hit exactly what the CMC move would hit. Prints the
		// blocking actor + component: our own VRCharPuppet, the rover, another pawn, or terrain geometry.
		FString BlockerName = TEXT("none");
		FString BlockerComp = TEXT("-");
		UWorld* World = GetWorld();
		UCapsuleComponent* Capsule = GetCapsuleComponent();
		if (World && Capsule && Speed > 5.0f)
		{
			const FVector Dir      = Velocity.GetSafeNormal();
			const FVector ProbeEnd = ActorLoc + Dir * 20.0f;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(VRBlockerProbe), /*bTraceComplex=*/false, this);
			FHitResult BlockHit;
			const FCollisionShape Shape = FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight());
			if (World->SweepSingleByProfile(BlockHit, ActorLoc, ProbeEnd, GetActorQuat(), Capsule->GetCollisionProfileName(), Shape, Params))
			{
				BlockerName = BlockHit.GetActor() ? BlockHit.GetActor()->GetName() : TEXT("unknown");
				BlockerComp = BlockHit.GetComponent() ? BlockHit.GetComponent()->GetName() : TEXT("-");
			}
		}
		UE_LOG(LogTemp, Warning, TEXT("[VRBlock]  Blocker=%s Comp=%s"), *BlockerName, *BlockerComp);

		--MotionLogBurstFrames;
	}

	PrevActorLocation     = ActorLoc;
	PrevHmdPos            = HmdPos;
	PrevCameraWorld       = CamWorld;
	PrevCapsuleHalfHeight = HalfHeight;
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

	// 0) Which instance is this and which branch of Tick is it taking? Tells us whether the C++ VR
	//    logic is even active here (bIsPossessed / bLocalVRSetupApplied) vs. everything being driven
	//    by the Blueprint OffsetActorToHMD / ScaleCapsuleToHMD instead.
	//UE_LOG(LogTemp, Warning, TEXT("[VRXform] STATE %s  bIsPossessed=%d  bLocalVRSetupApplied=%d  IsLocallyControlled=%d  LocalRole=%d  NetMode=%d"), *GetName(), bIsPossessed ? 1 : 0, bLocalVRSetupApplied ? 1 : 0, IsLocallyControlled() ? 1 : 0, (int32)GetLocalRole(), (int32)GetNetMode());

	// 1) The actor (capsule root). If the rig were rotated to the surface normal, ActorUp would be
	//    the diagonal moon-up vector — NOT (0,0,1). If it prints (0,0,1) the actor is NOT rotated.
	//UE_LOG(LogTemp, Warning, TEXT("[VRXform] ActorRot[%s]  ActorUp=%s"), *R(GetActorRotation()), *V(GetActorUpVector()));

	// 2) VROrigin — should inherit the actor rotation (its Up should match ActorUp).
	if (CachedVROrigin)
	{
		//UE_LOG(LogTemp, Warning, TEXT("[VRXform] VROrigin  WorldRot[%s]  WorldUp=%s  RelLoc=%s"), *R(CachedVROrigin->GetComponentRotation()), *V(CachedVROrigin->GetUpVector()), *V(CachedVROrigin->GetRelativeLocation()));
	}
	else
	{
		//UE_LOG(LogTemp, Warning, TEXT("[VRXform] VROrigin = NULL (tag not resolved)"));
	}

	// 3) VRCamera — this is what you actually see through. If its WorldUp stays ~(0,0,1) while the
	//    actor/VROrigin Up is the diagonal moon-up, then the HMD view is NOT inheriting the tilt
	//    (the real bug). If its WorldUp matches ActorUp, orientation is being inherited correctly.
	if (CachedVRCamera)
	{
		//UE_LOG(LogTemp, Warning, TEXT("[VRXform] VRCamera  WorldRot[%s]  WorldUp=%s  RelRot[%s]  LockToHmd=%d"), *R(CachedVRCamera->GetComponentRotation()), *V(CachedVRCamera->GetUpVector()),*R(CachedVRCamera->GetRelativeRotation()),CachedVRCamera->bLockToHmd ? 1 : 0);
	}
	else
	{
		//UE_LOG(LogTemp, Warning, TEXT("[VRXform] VRCamera = NULL (tag not resolved)"));
	}

	// 3b) WORLD POSITIONS. The camera should sit ~real-head-height ABOVE the feet ALONG the surface
	//     normal. HeightAlongUp = (camera - VROrigin) . SurfaceUp — expect roughly +120 (cm). If it's
	//     negative or tiny, the head offset is being placed in the wrong direction (e.g. the base
	//     orientation rotated the up-offset downward), which drops the camera below the legs.
	if (CachedVROrigin && CachedVRCamera)
	{
		const FVector ActorLoc  = GetActorLocation();
		const FVector OriginLoc = CachedVROrigin->GetComponentLocation();
		const FVector CamLoc    = CachedVRCamera->GetComponentLocation();
		const FVector SurfUp    = GetActorUpVector();
		const float   HeightAlongUp = FVector::DotProduct(CamLoc - OriginLoc, SurfUp);
		// UE_LOG(LogTemp, Warning, TEXT("[VRXform] Pos  Actor=%s  VROrigin(feet)=%s  VRCamera=%s  HeightAlongUp=%.1f"), *V(ActorLoc), *V(OriginLoc), *V(CamLoc), HeightAlongUp);

		// Mannequin scale check: how far the mesh's 'head' bone sits ABOVE the feet, along the surface
		// normal. Compare to HeightAlongUp (your real eye height). If the mesh head is much larger
		// (e.g. 230+), the mannequin is oversized and no room-scale height will reach its eyes.
		if (USkeletalMeshComponent* BodyMesh = GetMesh())
		{
			const FVector FeetW    = OriginLoc; // VROrigin is at the feet
			const FVector HeadBoneW = BodyMesh->GetSocketLocation(TEXT("head"));
			const float   MeshHeadAboveFeet = FVector::DotProduct(HeadBoneW - FeetW, SurfUp);
			//UE_LOG(LogTemp, Warning, TEXT("[VRXform] MeshHeadAboveFeet=%.1f  (compare to your HeightAlongUp=%.1f; capsuleHalfHeight=%.1f)"), MeshHeadAboveFeet, HeightAlongUp, GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.0f);
		}
	}

	// 4) Raw HMD pose (tracking space). This is what the runtime reports before the parent transform
	//    is applied — HmdRot is relative to the tracking origin, so its Up is real-world up.
	if (UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled())
	{
		FRotator HmdRot;
		FVector  HmdPos;
		UHeadMountedDisplayFunctionLibrary::GetOrientationAndPosition(HmdRot, HmdPos);
		//UE_LOG(LogTemp, Warning, TEXT("[VRXform] HMD raw   Rot[%s]  Pos=%s  LocalSetupApplied=%d"), *R(HmdRot), *V(HmdPos), bLocalVRSetupApplied ? 1 : 0);
	}
	else
	{
		//UE_LOG(LogTemp, Warning, TEXT("[VRXform] HMD not enabled  LocalSetupApplied=%d"), bLocalVRSetupApplied ? 1 : 0);
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

void ACharVR::SetIsPossessed(const bool InIsPossessed)
{
	bIsLocallyPossessed = InIsPossessed;
}

void ACharVR::SetShouldReplicateTransform(bool bReplicateTransform)
{
	bShouldReplicateTransform = bReplicateTransform;
}

void ACharVR::SetVRCharPuppet(AVRCharPuppet* VRCharPuppet)
{
	ARPuppet = VRCharPuppet;
}

AVRCharPuppet* ACharVR::GetPuppet()
{
	return ARPuppet;
}

void ACharVR::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UE_LOG(LogTemp, Error, TEXT("[ACharVR] EndPlay on %s, reason=%d, Controller=%s"),
		*GetName(), (int32)EndPlayReason, *GetNameSafe(GetController()));
	Super::EndPlay(EndPlayReason);
}

void ACharVR::ToggleVRCharPuppet(bool bShouldActivate)
{
	if (!ARPuppet)
	{
		return; 
	}
	
	ARPuppet->SetActorHiddenInGame(!bShouldActivate);
	ARPuppet->SetActorEnableCollision(bShouldActivate);
}
