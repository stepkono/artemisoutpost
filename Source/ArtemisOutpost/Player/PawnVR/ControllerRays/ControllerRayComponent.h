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

// Runtime state for one hand's ray. The interaction component is authored in BP_VRChar and only
// resolved here; the Niagara visual is created at runtime so its asset (the "design") can be swapped.
USTRUCT()
struct FControllerRayState
{
	GENERATED_BODY()

	EControllerRayHand Hand = EControllerRayHand::Right;

	// Which RayDesigns entry this hand currently shows. Per-hand so e.g. only the grabbing hand can
	// switch design while the other stays on the default.
	int32 DesignIndex = 0;

	// Configured in BP_VRChar (its debug ray already hits WBP_MiniGameConnectionUI). Resolved by tag
	// so the designer keeps ownership of interaction distance / trace channel.
	UPROPERTY(Transient)
	TObjectPtr<UWidgetInteractionComponent> Interaction = nullptr;

	// Created at runtime and attached under Interaction. Its asset is the current design; swapping the
	// asset switches designs. Purely visual — never replicated.
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> Visual = nullptr;

	bool bEnabled = true;
};

/**
 * Permanent laser-pointer rays out of the VR controllers for interacting with WORLD-space widgets.
 *
 * Client-local and non-replicated: a UWidgetInteractionComponent per hand (authored + configured in
 * BP_VRChar, resolved here by tag) does the actual trace + pointer events; this component only drives
 * the VISUAL — a Niagara system fed a [Start, End] point array each frame — and forwards the trigger
 * to Press/ReleasePointerKey. The visual "design" is one of RayDesigns and can be switched at runtime.
 *
 * Separate from the two other controller systems: the Tools-HUD (joystick navigation, no pointer) and
 * the hand tools (building arc). The RIGHT ray shares its controller with those, so callers should
 * suppress it via SetRayEnabled while a hand tool is active or the HUD is open; the LEFT ray is
 * normally permanent.
 *
 * NOTE (future): a shared "show others where I'm pointing" ray is a separate, opt-in feature. Because
 * the pawn is effectively server-driven for RPC purposes (project convention: client->server input
 * routes through the client-owned APawnController), that replicated design belongs on APawnController,
 * not here. This component stays purely local.
 */
UCLASS(ClassGroup = (ControllerRays), meta = (BlueprintSpawnableComponent))
class ARTEMISOUTPOST_API UControllerRayComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UControllerRayComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Called from ACharVR::SetupPlayerInputComponent (local player only). Binds the per-hand click to
	// the WidgetInteraction pointer. The mapping context carrying these actions is managed in Blueprint
	// (BP_PawnController) so it can be prioritised BELOW the HUD / hand-tool contexts — those consume
	// the trigger when active, so the ray click only fires when neither is holding it.
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

	// Switch the design on ONE hand only (e.g. grab-and-hold on that controller swaps just its ray).
	UFUNCTION(BlueprintCallable, Category = "Controller Rays")
	void SetHandDesign(EControllerRayHand Hand, int32 DesignIndex);

	// Cycle BOTH rays to the next design (wraps). Handy to bind to a button to try designs in-headset.
	UFUNCTION(BlueprintCallable, Category = "Controller Rays")
	void NextDesign();

	UFUNCTION(BlueprintPure, Category = "Controller Rays")
	int32 GetHandDesignIndex(EControllerRayHand Hand) const;

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

	// Tags on the WidgetInteractionComponents authored under each controller in BP_VRChar.
	UPROPERTY(EditDefaultsOnly, Category = "Controller Rays|Setup")
	FName LeftInteractionTag = TEXT("Ray_Left");

	UPROPERTY(EditDefaultsOnly, Category = "Controller Rays|Setup")
	FName RightInteractionTag = TEXT("Ray_Right");

	// Niagara User parameters the designs expose. Names must match the User parameters in the systems.
	//   PointArray (Vector array) = the [Start, End] polyline (world space) — the beam path.
	//   EndPoint   (Vector)       = the beam's end / impact point — position an endpoint sphere here.
	//   HitState   (float)        = 1 when the ray is on a widget else 0 — wire to colour.
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
	// Deferred until local control exists (networked possession lands after BeginPlay). Resolves the
	// interaction components and spawns the visuals. Returns true once at least one ray is set up.
	bool EnsureSetup();

	// Per-frame: push [Start, End] + hit state into a ray's Niagara.
	void UpdateRayVisual(const FControllerRayState& Ray) const;

	// Apply a RayDesigns entry to a visual and (de)activate it accordingly.
	void ApplyDesignToVisual(UNiagaraComponent* Visual, int32 DesignIndex) const;

	FControllerRayState* FindRay(EControllerRayHand Hand);

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

	// Desired enable state per hand, so SetRayEnabled works before setup and survives (re)setup.
	bool bDesiredLeftEnabled = true;
	bool bDesiredRightEnabled = true;

	bool bSetupDone = false;
};
