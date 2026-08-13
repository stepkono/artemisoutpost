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
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnToolActionDispatched, EToolAction, Action);

UCLASS(ClassGroup = (ToolsHUD), meta = (BlueprintSpawnableComponent))
class ARTEMISOUTPOST_API UToolsHUDComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UToolsHUDComponent();

	// Called from ACharVR::SetupPlayerInputComponent (local player only). Binds navigate + confirm.
	// The toggle button and the mapping-context swap are handled in Blueprint (BP_PawnController).
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

	UPROPERTY(BlueprintAssignable, Category = "Tools HUD")
	FOnToolActionDispatched OnToolActionDispatched;

protected:
	// Availability provider. Default returns true (no resource system yet). Override in a BP child of
	// this component — or replace the C++ body — to gate tiles on real, replicated resources.
	UFUNCTION(BlueprintNativeEvent, Category = "Tools HUD")
	bool IsActionAvailable(EToolAction Action, FText& OutReason);
	virtual bool IsActionAvailable_Implementation(EToolAction Action, FText& OutReason);

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

	// Right thumbstick (2D axis). Steps the highlight while the menu is open. The mapping contexts and
	// the toggle button are managed in Blueprint (BP_PawnController); this component only binds
	// navigate + confirm.
	UPROPERTY(EditDefaultsOnly, Category = "Tools HUD|Input")
	TObjectPtr<UInputAction> NavigateAction;

	// Trigger. Confirms the highlighted tile.
	UPROPERTY(EditDefaultsOnly, Category = "Tools HUD|Input")
	TObjectPtr<UInputAction> ConfirmAction;

	// ---- Bring-up test knob ----

	// Force tile enable-states without a resource system. Any action present here uses this bool
	// (checked BEFORE IsActionAvailable). Use it to verify greying-out end-to-end today; remove the
	// entries once real gating exists.
	UPROPERTY(EditAnywhere, Category = "Tools HUD|Debug")
	TMap<EToolAction, bool> DebugAvailabilityOverride;

private:
	// Input handlers.
	void HandleNavigateInput(const FInputActionValue& Value);
	void HandleConfirmInput();

	// Widget -> component: the player confirmed an enabled tile.
	UFUNCTION()
	void HandleToolActionConfirmed(EToolAction Action);

	// Gating bridge given to the widget (widget stays ignorant of resources / debug knobs).
	bool QueryAvailability(EToolAction Action, FText& OutReason);

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
