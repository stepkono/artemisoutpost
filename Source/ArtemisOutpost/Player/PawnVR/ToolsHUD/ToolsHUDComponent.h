// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ToolsHUDTypes.h"
#include "ToolsHUDComponent.generated.h"

class UToolsHUDWidget;
class UWidgetComponent;
class UInputAction;
class UEnhancedInputComponent;
struct FInputActionValue;

// Fired when the player confirms a gameplay tile (a Build* or Scan* action). The menu has already
// closed by the time this fires. Gameplay BP / the placement system listens here to start the
// actual action (place habitat, run scan, ...). Page-navigation and Close actions are handled
// internally and never broadcast.
// Fired for EVERY confirmed (enabled) tile — page navigation, close, and gameplay actions alike.
// Bind this ONE event and switch on the action (activate tool, begin placement, holster, ...). The
// component still handles page nav / close internally; this is just the public hook.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHUDAction, EHUDAction, Action);

UCLASS(ClassGroup = (ToolsHUD), meta = (BlueprintSpawnableComponent))
class ARTEMISOUTPOST_API UToolsHUDComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UToolsHUDComponent();

	// Called from ACharVR::SetupPlayerInputComponent (local player only). Binds ONLY the thumbstick
	// navigation. The toggle + trigger are routed in Blueprint (one trigger, state-based); the trigger
	// calls ConfirmSelection() when the menu is open.
	void BindInput(UEnhancedInputComponent* EnhancedInputComponent);

	UFUNCTION(BlueprintCallable, Category = "Tools HUD")
	void OpenMenu();

	UFUNCTION(BlueprintCallable, Category = "Tools HUD")
	void CloseMenu();

	UFUNCTION(BlueprintCallable, Category = "Tools HUD")
	void ToggleMenu();

	UFUNCTION(BlueprintPure, Category = "Tools HUD")
	bool IsMenuOpen() const { return bIsOpen; }

	// Re-evaluate tile gating while the menu is open (call when resources change).
	UFUNCTION(BlueprintCallable, Category = "Tools HUD")
	void RefreshGating();

	// Confirm the currently highlighted tile. Call from BP_VRChar's IA_Trigger while IsMenuOpen() is
	// true. Handles page nav / close internally and fires OnHUDAction. No-op if the menu is closed.
	UFUNCTION(BlueprintCallable, Category = "Tools HUD")
	void ConfirmSelection();

	// Bind this on the COMPONENT (a stable subobject) — NOT on the widget via Get Widget. The widget
	// is created lazily and swapped onto the WidgetComponent, so a Get Widget binding can land on a
	// stale instance and never fire.
	UPROPERTY(BlueprintAssignable, Category = "Tools HUD")
	FOnHUDAction OnHUDAction;

protected:
	// Availability provider. Default returns true (no resource system yet). Override in a BP child of
	// this component — or replace the C++ body — to gate tiles on real, replicated resources.
	UFUNCTION(BlueprintNativeEvent, Category = "Tools HUD")
	bool IsActionAvailable(EHUDAction Action, FText& OutReason);
	virtual bool IsActionAvailable_Implementation(EHUDAction Action, FText& OutReason);

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ---- Designer-assigned assets ----

	// WBP_ToolsHUD (must reparent to UToolsHUDWidget).
	UPROPERTY(EditDefaultsOnly, Category = "Tools HUD|Setup")
	TSubclassOf<UToolsHUDWidget> WidgetClass;

	// Tag on the WidgetComponent placed under the right controller in BP_VRChar. The widget renders
	// into that component so the designer controls its exact VR placement / size in-editor.
	UPROPERTY(EditDefaultsOnly, Category = "Tools HUD|Setup")
	FName WidgetAnchorTag = TEXT("ToolsHUD_Anchor");

	// Right thumbstick (2D axis). Steps the highlight while the menu is open. Contexts + toggle + the
	// trigger are managed in Blueprint; this component only binds the thumbstick navigation.
	UPROPERTY(EditDefaultsOnly, Category = "Tools HUD|Input")
	TObjectPtr<UInputAction> NavigateAction;

	// ---- Bring-up test knob ----

	// Force tile enable-states without a resource system. Any action present here uses this bool
	// (checked BEFORE IsActionAvailable). Use it to verify greying-out end-to-end today; remove the
	// entries once real gating exists.
	UPROPERTY(EditAnywhere, Category = "Tools HUD|Debug")
	TMap<EHUDAction, bool> DebugAvailabilityOverride;

private:
	// Input handler (thumbstick navigation only).
	void HandleNavigateInput(const FInputActionValue& Value);

	// Runs the confirmed action: page nav / close internally, then fires OnHUDAction for BP.
	void HandleToolActionConfirmed(EHUDAction Action);

	// Gating bridge given to the widget (widget stays ignorant of resources / debug knobs).
	bool QueryAvailability(EHUDAction Action, FText& OutReason);

	// Lazily create the widget and bind it to the anchor WidgetComponent. Returns false if setup
	// assets are missing.
	bool EnsureWidget();

	// Resolve the anchor WidgetComponent (by tag) on the owning pawn.
	UWidgetComponent* ResolveAnchor() const;

	UPROPERTY(Transient)
	TObjectPtr<UToolsHUDWidget> ActiveWidget;

	UPROPERTY(Transient)
	TObjectPtr<UWidgetComponent> AnchorWidgetComp;

	bool bIsOpen = false;
};
