// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ControllerRayComponent.generated.h"

class UWidgetInteractionComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class UEnhancedInputComponent;
class UInputAction;

// Which controller a ray belongs to.
UENUM(BlueprintType)
enum class EControllerRayHand : uint8
{
	Left,
	Right
};

// Runtime state for one hand's ray. The interaction component is authored in the pawn BP and only
// resolved here; the Niagara visual is created at runtime so its asset (the "design") can be swapped.
USTRUCT()
struct FControllerRayState
{
	GENERATED_BODY()

	EControllerRayHand Hand = EControllerRayHand::Right;

	// Which RayDesigns entry this hand currently shows. Per-hand so e.g. only the pointing hand can
	// switch design while the other stays on the default.
	int32 DesignIndex = 0;

	// Configured in the pawn BP (its debug ray already hits WBP_MiniGameConnectionUI). Resolved by tag
	// so the designer keeps ownership of interaction distance / trace channel.
	UPROPERTY(Transient)
	TObjectPtr<UWidgetInteractionComponent> Interaction = nullptr;

	// Created at runtime and attached under Interaction. Its asset is the current design; swapping the
	// asset switches designs. Purely visual — never replicated.
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> Visual = nullptr;

	bool bEnabled = true;

	// ---- Pointer mode (the shared "look where I point" ray) ----
	bool bPointerMode = false;

	// What to restore when pointer mode ends.
	float SavedInteractionDistance = 0.0f;
	int32 SavedDesignIndex = 0;
	bool  bPointerForcedEnable = false;
};

/**
 * Laser-pointer rays out of the controllers for interacting with WORLD-space widgets, plus the
 * opt-in POINTER MODE that turns one hand's ray into the shared "look where I am pointing" cue.
 *
 * The trace is done by a UWidgetInteractionComponent per hand (authored + configured in the pawn BP,
 * resolved here by tag). This component only drives the VISUAL — a Niagara system fed a [Start, End]
 * point array each frame — and forwards the trigger to Press/ReleasePointerKey. The visual "design"
 * is one of RayDesigns and can be switched at runtime.
 *
 * Pointer mode (SetPointerMode) runs IDENTICAL code on every machine: it swaps that hand to
 * PointerDesignIndex and raises the interaction distance to PointerInteractionDistance, and restores
 * both when it ends. The owning client turns it on from input (via UPlayerCuesManager), every other
 * peer turns it on from the replicated AArtemisPlayerState, so a remote proxy draws the same laser
 * from its interpolated hand. Nothing here replicates.
 *
 * Tick guard: the locally controlled pawn always ticks (widget rays); any other instance ticks only
 * while a hand is in pointer mode. The listen-server host never draws (no headset).
 */
UCLASS(ClassGroup = (ControllerRays), meta = (BlueprintSpawnableComponent))
class ARTEMISOUTPOST_API UControllerRayComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UControllerRayComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Called from the pawn's SetupPlayerInputComponent (local player only). Binds the per-hand click to
	// the WidgetInteraction pointer. The mapping context carrying these actions is managed in Blueprint
	// (BP_PawnController) so it can be prioritised BELOW the HUD / hand-tool contexts.
	void BindInput(UEnhancedInputComponent* EnhancedInputComponent);

	// Show/hide + enable/disable one hand's ray. Use to suppress the RIGHT ray while a hand tool is
	// active or the HUD is open. Safe to call before setup completes — the state is remembered and
	// applied once the ray exists.
	UFUNCTION(BlueprintCallable, Category = "Controller Rays")
	void SetRayEnabled(EControllerRayHand Hand, bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Controller Rays")
	void SetAllRaysEnabled(bool bEnabled);

	// Switch the visual design (Niagara system) on BOTH rays. Index into RayDesigns.
	UFUNCTION(BlueprintCallable, Category = "Controller Rays")
	void SetDesign(int32 DesignIndex);

	// Switch the design on ONE hand only.
	UFUNCTION(BlueprintCallable, Category = "Controller Rays")
	void SetHandDesign(EControllerRayHand Hand, int32 DesignIndex);

	// Cycle BOTH rays to the next design (wraps). Handy to bind to a button to try designs in-headset.
	UFUNCTION(BlueprintCallable, Category = "Controller Rays")
	void NextDesign();

	UFUNCTION(BlueprintPure, Category = "Controller Rays")
	int32 GetHandDesignIndex(EControllerRayHand Hand) const;

	// ---- Pointer mode ----

	// Turn the shared pointer ray on/off for one hand. Idempotent; safe before setup (remembered).
	// Do NOT call this from input directly — go through UPlayerCuesManager::SetPointing so the state
	// reaches the server and the other peers. The cue manager calls this on every machine.
	UFUNCTION(BlueprintCallable, Category = "Controller Rays|Pointer")
	void SetPointerMode(EControllerRayHand Hand, bool bOn);

	UFUNCTION(BlueprintPure, Category = "Controller Rays|Pointer")
	bool IsPointerModeActive(EControllerRayHand Hand) const;

	// The current ray of one hand: origin, direction and the interaction component's last hit. Used by
	// UPlayerCuesManager on the owning client to resolve the pointing target. Returns false when the
	// hand's ray is not set up.
	bool GetRayHit(EControllerRayHand Hand, FVector& OutStart, FVector& OutDirection, FHitResult& OutHit) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ---- Designer-assigned setup ----

	// Selectable ray visuals. Each entry is one design (e.g. NS_ControllerRay); element 0 is default.
	// Every design must read the point array (default "PointArray") to draw the Start->End line.
	UPROPERTY(EditDefaultsOnly, Category = "Controller Rays|Setup")
	TArray<TObjectPtr<UNiagaraSystem>> RayDesigns;

	UPROPERTY(EditDefaultsOnly, Category = "Controller Rays|Setup")
	int32 DefaultDesignIndex = 0;

	// Tags on the WidgetInteractionComponents authored under each controller in the pawn BP.
	UPROPERTY(EditDefaultsOnly, Category = "Controller Rays|Setup")
	FName LeftInteractionTag = TEXT("Ray_Left");

	UPROPERTY(EditDefaultsOnly, Category = "Controller Rays|Setup")
	FName RightInteractionTag = TEXT("Ray_Right");

	// ---- Pointer mode ----

	// RayDesigns entry shown while a hand is in pointer mode (the "Design 2" look).
	UPROPERTY(EditDefaultsOnly, Category = "Controller Rays|Pointer")
	int32 PointerDesignIndex = 1;

	// Interaction distance (cm) applied to the pointing hand's WidgetInteractionComponent while in
	// pointer mode. Must be larger than the widget reach configured in the BP (750 today) so the
	// pointer reaches buildings far away. Restored on exit.
	UPROPERTY(EditDefaultsOnly, Category = "Controller Rays|Pointer", meta = (ClampMin = "100.0"))
	float PointerInteractionDistance = 10000.0f;

	// Feed PointArray / EndPoint in the Niagara COMPONENT's local space instead of world space. World
	// positions on the georeferenced moon are at ~1700 km, which a float Vector parameter cannot hold
	// at centimetre precision (LogNiagara "does not fit into a FVector3f" spam, beam collapses). In
	// local space the start is ~0 and the end a few metres away. OPT-IN: it REQUIRES every emitter in
	// the RayDesigns systems to be set to Local Space (emitter properties), otherwise the beam draws
	// at the world origin and every ray vanishes. Switch the assets first, then tick this.
	UPROPERTY(EditDefaultsOnly, Category = "Controller Rays|Niagara")
	bool bFeedPointsInLocalSpace = false;

	// Niagara User parameters the designs expose. Names must match the User parameters in the systems.
	UPROPERTY(EditDefaultsOnly, Category = "Controller Rays|Niagara")
	FName PointArrayParamName = TEXT("PointArray");

	UPROPERTY(EditDefaultsOnly, Category = "Controller Rays|Niagara")
	FName EndPointParamName = TEXT("EndPoint");

	UPROPERTY(EditDefaultsOnly, Category = "Controller Rays|Niagara")
	FName HitStateParamName = TEXT("HitState");

	// ---- Input ----

	UPROPERTY(EditDefaultsOnly, Category = "Controller Rays|Input")
	TObjectPtr<UInputAction> LeftClickAction;

	UPROPERTY(EditDefaultsOnly, Category = "Controller Rays|Input")
	TObjectPtr<UInputAction> RightClickAction;

private:
	// Deferred until the BP components exist. Resolves the interaction components and spawns the
	// visuals. Returns true once at least one ray is set up.
	bool EnsureSetup();

	// Per-frame: push [Start, End] + hit state into a ray's Niagara.
	void UpdateRayVisual(const FControllerRayState& Ray) const;

	// Apply a RayDesigns entry to a visual and (de)activate it accordingly.
	void ApplyDesignToVisual(UNiagaraComponent* Visual, int32 DesignIndex) const;

	// Apply the visual/interaction enable state to a ray (shared by SetRayEnabled and pointer mode).
	void ApplyEnabled(FControllerRayState& Ray, bool bEnabled) const;

	void ApplyPointerMode(FControllerRayState& Ray, bool bOn);

	FControllerRayState* FindRay(EControllerRayHand Hand);
	const FControllerRayState* FindRay(EControllerRayHand Hand) const;

	bool IsOwnerLocallyControlled() const;
	bool HasAnyPointerModeDesired() const { return bDesiredLeftPointer || bDesiredRightPointer; }

	// Input handlers -> drive the matching WidgetInteraction pointer.
	void PressPointer(EControllerRayHand Hand);
	void ReleasePointer(EControllerRayHand Hand);
	void HandleLeftPressed() { PressPointer(EControllerRayHand::Left); }
	void HandleLeftReleased() { ReleasePointer(EControllerRayHand::Left); }
	void HandleRightPressed() { PressPointer(EControllerRayHand::Right); }
	void HandleRightReleased() { ReleasePointer(EControllerRayHand::Right); }

	UPROPERTY(Transient)
	TArray<FControllerRayState> Rays;

	int32 CurrentDesignIndex = 0;

	// Desired states per hand, so the setters work before setup and survive (re)setup.
	bool bDesiredLeftEnabled = true;
	bool bDesiredRightEnabled = true;
	bool bDesiredLeftPointer = false;
	bool bDesiredRightPointer = false;

	bool bSetupDone = false;
};
