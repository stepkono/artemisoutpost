// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "PlayerCueTypes.h"
#include "PlayerCuesManager.generated.h"

class AArtemisPlayerState;
class APawnController;
class AGeoRefsManager;
class UControllerRayComponent;
class UCameraComponent;
class UEnhancedInputComponent;
class UInputAction;

/**
 * Producer of the awareness cues for ONE pawn (lives on both ACharVR and APawnAR).
 *
 * Roles per instance:
 *  - Owning client, active pawn: reads input (pointer left/right, talk), runs the pointer and gaze
 *    traces, resolves hits to FPointingTarget, converts to geo coords with this pawn's moon and sends
 *    throttled RPCs through the client-owned APawnController. Nothing spatial runs anywhere else.
 *  - Server instance: derives Walking from the position it already receives and writes it into the
 *    PlayerState.
 *  - Every instance: relays the replicated pointing state into UControllerRayComponent::SetPointerMode,
 *    so the same laser code runs on every machine.
 *
 * Input is bound from the pawn's SetupPlayerInputComponent (same pattern as the Tools-HUD and the
 * ray component), so it follows engine possession: the server-side mode switch unpossesses and
 * re-possesses the target pawn on every AR/VR switch.
 */
UCLASS(ClassGroup = (PlayerCues), meta = (BlueprintSpawnableComponent))
class ARTEMISOUTPOST_API UPlayerCuesManager : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayerCuesManager();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Called from the pawn's SetupPlayerInputComponent (local player only). Binds pointer left/right
	// (Started = on, Completed = off) and talk (hold). The actions live in the mapping context managed
	// on BP_PawnController.
	void BindInput(UEnhancedInputComponent* EnhancedInputComponent);

	// ---- Local intents (owning client). Safe to call from Blueprint. ----

	UFUNCTION(BlueprintCallable, Category = "Player Cues")
	void SetPointing(EPointingHand Hand, bool bOn);

	UFUNCTION(BlueprintCallable, Category = "Player Cues")
	void SetTalking(bool bOn);

	// Called by the hand tools on activate (their kind) and deactivate (None).
	UFUNCTION(BlueprintCallable, Category = "Player Cues")
	void ReportToolActivity(EToolActivity Tool);

	// Switches: when off, the hit is still traced locally (for the laser) but never leaves the client.
	UFUNCTION(BlueprintCallable, Category = "Player Cues")
	void SetReplicatePointerHit(bool bEnabled) { bReplicatePointerHit = bEnabled; }

	UFUNCTION(BlueprintCallable, Category = "Player Cues")
	void SetReplicateGazeHit(bool bEnabled) { bReplicateGazeHit = bEnabled; }

	// ---- Reads ----

	UFUNCTION(BlueprintPure, Category = "Player Cues")
	AArtemisPlayerState* GetLinkedPlayerState() const;

	UFUNCTION(BlueprintPure, Category = "Player Cues")
	bool IsPointing() const { return bLocalPointing; }

	// True when this pawn is locally controlled AND is the pawn for the current XR mode (AR pawn in
	// AR, VR char in VR). The mode check guards the frames between the mode variable flipping and
	// the re-possession landing.
	UFUNCTION(BlueprintPure, Category = "Player Cues")
	bool IsLocalActivePawn() const;

	// Which moon this pawn's hits are expressed against. Set by the pawn constructor (APawnAR = true).
	void SetUsesARMoon(bool bAR) { bUsesARMoon = bAR; }
	bool UsesARMoon() const { return bUsesARMoon; }

	// Resolve a trace hit to the abstract target (unwraps puppets, classifies, converts to geo).
	bool ResolveTarget(const FHitResult& Hit, FPointingTarget& OutTarget);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ---- Input (assign in the pawn BP defaults) ----

	UPROPERTY(EditDefaultsOnly, Category = "Player Cues|Input")
	TObjectPtr<UInputAction> PointerLeftAction;

	UPROPERTY(EditDefaultsOnly, Category = "Player Cues|Input")
	TObjectPtr<UInputAction> PointerRightAction;

	UPROPERTY(EditDefaultsOnly, Category = "Player Cues|Input")
	TObjectPtr<UInputAction> TalkAction;

	// ---- Pointer ----

	UPROPERTY(EditAnywhere, Category = "Player Cues|Pointer")
	bool bReplicatePointerHit = true;

	// Seconds between pointer target reports while pointing (unreliable RPC, only if changed).
	UPROPERTY(EditDefaultsOnly, Category = "Player Cues|Pointer", meta = (ClampMin = "0.02"))
	float PointerReportInterval = 0.1f;

	// ---- Gaze ----

	UPROPERTY(EditAnywhere, Category = "Player Cues|Gaze")
	bool bReplicateGazeHit = true;

	UPROPERTY(EditDefaultsOnly, Category = "Player Cues|Gaze", meta = (ClampMin = "0.05"))
	float GazeTraceInterval = 0.2f;

	// A new gaze target must be held this long before it is reported (kills glance flicker).
	UPROPERTY(EditDefaultsOnly, Category = "Player Cues|Gaze", meta = (ClampMin = "0.0"))
	float GazeDwellSeconds = 0.3f;

	UPROPERTY(EditDefaultsOnly, Category = "Player Cues|Gaze", meta = (ClampMin = "10.0"))
	float GazeTraceDistance = 10000.0f;

	// Tag of the HMD camera on this pawn (BP_VRChar tags it VR_Camera). Falls back to the first camera.
	UPROPERTY(EditDefaultsOnly, Category = "Player Cues|Gaze")
	FName GazeCameraTag = TEXT("VR_Camera");

	UPROPERTY(EditDefaultsOnly, Category = "Player Cues|Gaze")
	TEnumAsByte<ECollisionChannel> GazeTraceChannel = ECC_Visibility;

	// ---- Walking (server) ----

	// Speed (cm/s) above which the player counts as walking.
	UPROPERTY(EditDefaultsOnly, Category = "Player Cues|Walking", meta = (ClampMin = "0.0"))
	float WalkSpeedThreshold = 20.0f;

	// Seconds the new state must persist before it is committed.
	UPROPERTY(EditDefaultsOnly, Category = "Player Cues|Walking", meta = (ClampMin = "0.0"))
	float WalkStateHysteresis = 0.5f;

	// Seconds between speed samples on the server (the pawn position arrives at tracker frequency).
	UPROPERTY(EditDefaultsOnly, Category = "Player Cues|Walking", meta = (ClampMin = "0.05"))
	float WalkSampleInterval = 0.25f;

	// ---- Moon ----
	UPROPERTY(EditDefaultsOnly, Category = "Player Cues")
	bool bUsesARMoon = false;

private:
	// Input handlers (gated on IsLocalActivePawn).
	void HandlePointerLeftStarted()   { HandlePointerInput(EPointingHand::Left, true); }
	void HandlePointerLeftCompleted() { HandlePointerInput(EPointingHand::Left, false); }
	void HandlePointerRightStarted()   { HandlePointerInput(EPointingHand::Right, true); }
	void HandlePointerRightCompleted() { HandlePointerInput(EPointingHand::Right, false); }
	void HandleTalkStarted()   { HandleTalkInput(true); }
	void HandleTalkCompleted() { HandleTalkInput(false); }
	void HandlePointerInput(EPointingHand Hand, bool bOn);
	void HandleTalkInput(bool bOn);

	// Owning-client work.
	void TickLocalPointer(float DeltaTime);
	void TickLocalGaze(float DeltaTime);

	// Server work.
	void TickServerWalking(float DeltaTime);

	// PlayerState plumbing: resolve lazily (replication order), bind once, apply the current state.
	bool EnsurePlayerState();

	UFUNCTION()
	void HandleCueStateChanged(const FPlayerCueState& State);

	// Relay the replicated pointing state into the ray component (every machine).
	void ApplyPointerVisual(const FPlayerCueState& State);

	// True when this pawn is the one the state's context refers to (VR char for VR, AR pawn for AR).
	bool MatchesContext(EPlayerContext Context) const;

	APawnController* GetLocalController() const;
	UControllerRayComponent* GetRayComponent() const;
	UCameraComponent* GetGazeCamera();
	AGeoRefsManager* GetGeoRefsManager();
	FVector WorldToGeo(const FVector& World);

	UPROPERTY(Transient)
	TWeakObjectPtr<AArtemisPlayerState> CachedPlayerState;

	UPROPERTY(Transient)
	TObjectPtr<AGeoRefsManager> CachedGeoRefs;

	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> CachedGazeCamera;

	bool bBoundToPlayerState = false;

	// Local pointer state.
	bool bLocalPointing = false;
	EPointingHand LocalPointingHand = EPointingHand::Left;
	float PointerReportAccum = 0.0f;
	FPointingTarget LastReportedPointer;
	bool bHasReportedPointer = false;

	// Local talk state (two hands could hold a shared action; only flips on real changes).
	bool bLocalTalking = false;

	// Local gaze state.
	float GazeTraceAccum = 0.0f;
	FPointingTarget GazeCandidate;
	float GazeCandidateHeld = 0.0f;
	FPointingTarget LastReportedGaze;

	// Server walking state.
	FVector LastServerPos = FVector::ZeroVector;
	double LastServerSampleTime = 0.0;
	bool bHasServerSample = false;
	bool bServerWalking = false;
	float WalkStateTimer = 0.0f;
};
