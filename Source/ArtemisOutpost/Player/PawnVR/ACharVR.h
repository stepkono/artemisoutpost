// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Cesium3DTileset.h"
#include "CesiumGeoreference.h"
#include "ArtemisOutpost/Moon/Cesium/GeoRefsManager.h"
#include "GameFramework/Character.h"
#include "ACharVR.generated.h"

class UToolsHUDComponent;
class UControllerRayComponent;
class UWidgetComponent;
class UUserWidget;
class AHandToolBase;

UCLASS()
class ARTEMISOUTPOST_API ACharVR : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ACharVR();
	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
	void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	
	UFUNCTION(BlueprintCallable, Category = "VR Moon")
	ACesium3DTileset* GetVRTileset(); 
	
	UFUNCTION(BlueprintCallable, Category = "GeoRefsManager")
	void SetGeoRefsManager(AGeoRefsManager* InManager);
	
	UFUNCTION(BlueprintCallable, Category = "Possession")
	void SetIsPossessed(const bool InIsPossessed);
	
	UFUNCTION(BlueprintCallable, Category = "Replication")
	void SetShouldReplicateTransform(bool bReplicateTransform);

	// The hand tool the trigger currently drives (set by AHandToolBase::ActivateTool/DeactivateTool).
	// BP_VRChar routes IA_Trigger to GetActiveTool()->ExecuteAction()/EndAction().
	UFUNCTION(BlueprintPure, Category = "Hand Tools")
	AHandToolBase* GetActiveTool() const { return ActiveHandTool; }

	void SetActiveHandTool(AHandToolBase* Tool) { ActiveHandTool = Tool; }

	// World-space minigame HUD in front of the HMD (a WidgetComponent authored on BP_VRChar under
	// the camera). The minigame controller hands over the created View here on join; a dark sphere
	// behind it dims the surroundings. Screen-space (AddToViewport) is avoided because it does not
	// render in the HMD.
	void ShowMinigameView(UUserWidget* Widget);
	void HideMinigameView();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void NotifyControllerChanged() override;

	// Refuses Cesium streamed tiles as a movement base. They are created at runtime and are not
	// network-addressable, so CMC's base-relative replication can't resolve them across the wire
	// (spams FNetGUIDCache::SupportsObject "NOT Supported" and drops every ClientAdjustPosition
	// correction). Nulling the base makes the character replicate ABSOLUTE position instead, which
	// both peers resolve because the moon is at the same georeferenced UE location everywhere.
	virtual void SetBase(UPrimitiveComponent* NewBase, FName BoneName = NAME_None, bool bNotifyActor = true) override;
		
	UFUNCTION(BlueprintImplementableEvent, Category = "MiniGame")
	void MiniGameIsOpen(); 
	
	UFUNCTION(BlueprintImplementableEvent, Category = "MiniGame")
	void MiniGameIsClosed();
	
private:
	// Resolves VROrigin/VRCamera (by tag), caches them, locks the camera to the HMD and computes
	// the feet-level origin offset. Safe on every instance (server/proxy/local) — does NOT touch
	// the global HMD tracking origin. Call once in BeginPlay.
	void InitComponentsFromBP();

	// The local-player-only part of the VR setup: re-homes VROrigin to the feet and switches the
	// HMD to floor-level tracking. Guarded by IsLocallyControlled() + HMD and a one-shot flag, so
	// it can be called from both BeginPlay and NotifyControllerChanged — whichever wins the race
	// once local control is actually established (in networked play, possession happens AFTER
	// BeginPlay, so BeginPlay alone is too early).
	void ApplyLocalVRSetup();

	// Throttled diagnostic: logs the real world/relative rotations of the actor, VROrigin, VRCamera
	// and the raw HMD pose so we can see exactly which node does (or does not) inherit the rig tilt.
	void LogVRTransforms(float DeltaTime);

	// Rotates the XR tracking space each frame so the rendered horizon aligns to the pawn's up
	// (moon surface normal). This is the ONLY way to tilt the VR horizon — the compositor ignores
	// the camera component orientation (see bAlignVRViewToSurface). Local + HMD only.
	void UpdateVRViewTilt();

	// Slides the capsule under the HMD every frame (and counter-slides the VR origin so the view
	// doesn't jump) so the player can never physically walk out of their own body. This is what
	// turns a free-floating VR camera into an actual VR character. Local + HMD only.
	void UpdateCapsuleFollowsHMD();

	// Per-frame roof/ceiling avoidance for the HMD view. Local + HMD only.
	void UpdateHeadCollision(float DeltaTime);

protected:
	UPROPERTY(BlueprintReadWrite, Category = "Player Height")
	float Height = 180;
	
	UPROPERTY(BlueprintReadWrite, Category = "Replication")
	bool bShouldReplicateTransform = false;
	
	UPROPERTY(BlueprintReadOnly, Category = "VR Moon")
	ACesium3DTileset* VRTileSet;
	
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "GeoRefsManager")
	AGeoRefsManager* GeoRefsManager;
	
	// ---- VR component tags (must match the tags set on the components in BP_VRChar) ----
	UPROPERTY(EditDefaultsOnly, Category = "VR")
	FName VROriginTag = TEXT("VR_Origin");

	UPROPERTY(EditDefaultsOnly, Category = "VR")
	FName VRCameraTag = TEXT("VR_Camera");

	// Tags of the world-space minigame HUD holder + the dimming sphere, authored on BP_VRChar under
	// the camera (both hidden by default; C++ resolves them by tag and toggles them).
	UPROPERTY(EditDefaultsOnly, Category = "VR")
	FName MinigameViewTag = TEXT("Minigame_View");

	UPROPERTY(EditDefaultsOnly, Category = "VR")
	FName MinigameDimTag = TEXT("Minigame_Dim");

	// ---- VR surface-alignment (tilt the HMD horizon to the moon surface normal) ----
	// The XR compositor renders the head pose in a WORLD/gravity-up tracking space and ignores the
	// pawn/camera component orientation, so on the georeferenced globe the horizon stays world-up
	// (you appear to lie on the tilted surface). We fix this by rotating the tracking space itself
	// every frame via SetBaseRotation, aligning world-up to the pawn's up (surface normal). The HMD
	// still controls look direction freely within that tilted frame, and the tilt follows the player
	// as they move across the globe.
	UPROPERTY(EditDefaultsOnly, Category = "VR|Surface Alignment")
	bool bAlignVRViewToSurface = true;

	// Selects the sign of the base-orientation convention (base = tilt vs tilt.Inverse), which
	// differs between XR runtimes. Defaults to TRUE because the Oculus/OpenXR runtime applies base
	// orientation as an INVERSE (recenter convention): reported = Base^-1 * device, so we must feed
	// TiltQ.Inverse() to get camera-up = surface normal. Editable at runtime to flip live in PIE.
	UPROPERTY(EditAnywhere, Category = "VR|Surface Alignment")
	bool bInvertVRViewTilt = true;

	// ---- Head / roof collision (keeps the HMD view from clipping through geometry above) ----
	// When true, if the player's real head rises into a ceiling the VR view is pushed back
	// down so the camera stays below the obstruction instead of seeing through it.
	UPROPERTY(EditDefaultsOnly, Category = "VR|Head Collision")
	bool bEnableHeadCollision = true;

	// Radius (cm) of the sphere swept up to the head; roughly the player's head half-width.
	UPROPERTY(EditDefaultsOnly, Category = "VR|Head Collision", meta = (ClampMin = "1.0"))
	float HeadCollisionRadius = 12.0f;

	// How quickly the push-down offset eases in/out. Higher = snappier, lower = smoother
	UPROPERTY(EditDefaultsOnly, Category = "VR|Head Collision", meta = (ClampMin = "0.0"))
	float HeadCollisionInterpSpeed = 15.0f;
	
	UPROPERTY(BlueprintReadOnly, Category = "Possession")
	bool bIsPossessed = false;

	// Client-local wrist Tools-HUD (open/close, navigate, dispatch). Created in the ctor; only does
	// anything on the locally-controlled pawn. See UToolsHUDComponent.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tools HUD")
	TObjectPtr<UToolsHUDComponent> ToolsHUDComponent;

	// Client-local laser-pointer rays out of both controllers for world-space widget interaction. The
	// WidgetInteractionComponents it drives are authored/tagged in BP_VRChar. See UControllerRayComponent.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Controller Rays")
	TObjectPtr<UControllerRayComponent> ControllerRayComponent;

	// The hand tool the trigger currently drives (Building/Scanning). Maintained by AHandToolBase's
	// Activate/DeactivateTool; read via GetActiveTool() from BP_VRChar's trigger routing.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Hand Tools", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AHandToolBase> ActiveHandTool;
	
	UPROPERTY(BlueprintReadWrite, Category = "Minigame")
	bool bIsInMiniGame = false;

private:
	// One-shot guard so the local floor/origin setup is applied exactly once.
	bool bLocalVRSetupApplied = false;

	// Accumulator to throttle LogVRTransforms to ~1 Hz.
	float VRDebugLogTimer = 0.0f;
	
	UPROPERTY()
	USkeletalMeshComponent* SkeletalMesh; 

	// VR rig, resolved from BP_VRChar by tag in BeginPlay.
	// NOTE: deliberately NOT named VROrigin/VRCamera — those names belong to the components
	// authored in BP_VRChar, and matching them would collide with the generated BP properties.
	UPROPERTY(Transient)
	USceneComponent* CachedVROrigin = nullptr;
	
	UPROPERTY()
	bool bXRBaseIsReset = false; 

	UPROPERTY(Transient)
	class UCameraComponent* CachedVRCamera = nullptr;

	// World-space minigame HUD holder + dimming sphere, resolved by tag from BP_VRChar.
	UPROPERTY(Transient)
	UWidgetComponent* CachedMinigameView = nullptr;

	UPROPERTY(Transient)
	USceneComponent* CachedMinigameDim = nullptr;

	// Relative Z of VROrigin that places it at the bottom of the capsule (the player's feet),
	// i.e. the floor-level tracking origin. Computed once from the capsule half-height.
	float VROriginBaseZ = 0.0f;

	// Smoothed downward offset (cm) currently applied for head/roof collision.
	float CurrentHeadCollisionPush = 0.0f;
};
