// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlayerInputModeComponent.generated.h"

class AActor;
class ACharVR;
class AHandToolBase;
class UEnhancedInputComponent;
class UEnhancedInputLocalPlayerSubsystem;
class UInputAction;
class UInputMappingContext;
class UWidgetInteractionComponent;

// What the local VR player is doing right now. Mutually exclusive: entering one tears the previous one
// down. Named EInputActivity because EPlayerActivity already exists for the awareness cues.
UENUM(BlueprintType)
enum class EInputActivity : uint8
{
	Free     UMETA(DisplayName = "Free"),
	HUDMenu  UMETA(DisplayName = "HUD Menu"),
	HandTool UMETA(DisplayName = "Hand Tool"),
	MiniGame UMETA(DisplayName = "Mini Game")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInputActivityChanged, EInputActivity, OldActivity, EInputActivity, NewActivity);

/**
 * The one owner of the local VR player's input activity, on APawnController (client-local, never
 * replicated, inert on non-local controllers).
 *
 * - SetActivity is the ONLY writer of the activity and the ONLY place that adds or removes the overlay
 *   mapping contexts (IMC_HUD, IMC_MiniGame). The locomotion context (IMC_VRChar) stays with the AR/VR
 *   switch in BP_PawnController and is never touched here.
 * - The HUD component, the hand tools and the minigame controller REPORT into it. Nobody else ranks
 *   flags against each other.
 * - HandleTrigger is the single trigger entry. A press on a world widget (hovered by the right ray, not
 *   owned by the own pawn) always wins, then the activity decides.
 */
UCLASS(ClassGroup = (Input), meta = (BlueprintSpawnableComponent))
class ARTEMISOUTPOST_API UPlayerInputModeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayerInputModeComponent();

	// The component of the local player that owns or controls this actor (a pawn, a controller, or an
	// actor owned by either). Null when there is none, for example on a remote proxy.
	static UPlayerInputModeComponent* FindFor(const AActor* Actor);

	// Called from ACharVR::SetupPlayerInputComponent (local player only). Binds TriggerAction to
	// HandleTrigger: Started = press, Completed / Canceled = release.
	void BindInput(UEnhancedInputComponent* EnhancedInputComponent);

	// Tears the previous activity down, enters the new one, swaps the overlay contexts, logs
	// "[InputMode] Old -> New (Reason)" and fires OnActivityChanged. Refused (logged) when CanEnter is
	// false. Nested calls made by a teardown are ignored on purpose (re-entrancy guard).
	UFUNCTION(BlueprintCallable, Category = "Input Mode")
	void SetActivity(EInputActivity NewActivity, const FString& Reason);

	// Goes to Free, but only if the current activity is still Expected. For "I am done" reports (HUD
	// closed, tool holstered, View closed) that must not clobber an activity entered in between.
	UFUNCTION(BlueprintCallable, Category = "Input Mode")
	void LeaveActivity(EInputActivity Expected, const FString& Reason);

	// A hand tool is being drawn. Holsters any other drawn tool first, then enters HandTool. Call it
	// BEFORE the tool registers itself as the pawn's active tool (AHandToolBase::ActivateTool does).
	void EnterHandTool(AHandToolBase* Tool);

	UFUNCTION(BlueprintPure, Category = "Input Mode")
	EInputActivity GetActivity() const { return Activity; }

	// The rule set: the HUD cannot be opened while inside a minigame. Everything else may be entered.
	UFUNCTION(BlueprintPure, Category = "Input Mode")
	bool CanEnter(EInputActivity NewActivity) const;

	// Live, never stored: the right ray (WidgetInteraction tagged RightRayTag) is over an interactable
	// widget on a WidgetComponent that does NOT belong to the own pawn (the HUD anchor and the minigame
	// View holder live on the pawn and are driven by HUD / minigame input instead).
	UFUNCTION(BlueprintPure, Category = "Input Mode")
	bool IsHoveringWorldWidget() const;

	// The single trigger entry. Press: world widget hover -> pointer click, else HUDMenu ->
	// ConfirmSelection, MiniGame -> SubmitEnterAction, HandTool -> ExecuteAction, Free -> nothing.
	// Release goes to whatever handled the press, even if the ray or the activity changed since.
	UFUNCTION(BlueprintCallable, Category = "Input Mode")
	void HandleTrigger(bool bPressed);

	// Presentation hook for BP (for example hide the right controller mesh while a tool is drawn).
	UPROPERTY(BlueprintAssignable, Category = "Input Mode")
	FOnInputActivityChanged OnActivityChanged;

protected:
	// IMC_HUD. Present only in HUDMenu.
	UPROPERTY(EditDefaultsOnly, Category = "Input Mode|Contexts")
	TObjectPtr<UInputMappingContext> HUDContext;

	UPROPERTY(EditDefaultsOnly, Category = "Input Mode|Contexts")
	int32 HUDContextPriority = 10;

	// IMC_MiniGame. Present only in MiniGame.
	UPROPERTY(EditDefaultsOnly, Category = "Input Mode|Contexts")
	TObjectPtr<UInputMappingContext> MiniGameContext;

	UPROPERTY(EditDefaultsOnly, Category = "Input Mode|Contexts")
	int32 MiniGameContextPriority = 10;

	// IA_TriggerRight (lives in IMC_VRChar).
	UPROPERTY(EditDefaultsOnly, Category = "Input Mode|Input")
	TObjectPtr<UInputAction> TriggerAction;

	// Tag of the right WidgetInteractionComponent on BP_VRChar.
	UPROPERTY(EditDefaultsOnly, Category = "Input Mode|Input")
	FName RightRayTag = TEXT("Ray_Right");

private:
	enum class ETriggerRoute : uint8
	{
		None,
		WorldWidget,
		HUD,
		MiniGame,
		HandTool
	};

	void HandleTriggerPressed() { HandleTrigger(true); }
	void HandleTriggerReleased() { HandleTrigger(false); }

	// Per-activity teardown, run before the new activity is entered.
	void TearDown(EInputActivity Leaving);

	// Add / remove the overlay contexts so exactly the ones of the current activity are present.
	void ApplyContexts() const;

	bool IsLocal() const;
	ACharVR* GetVRChar() const;
	UWidgetInteractionComponent* GetRightRay() const;
	UEnhancedInputLocalPlayerSubsystem* GetInputSubsystem() const;
	FString DescribeContexts() const;

	EInputActivity Activity = EInputActivity::Free;
	ETriggerRoute PressedRoute = ETriggerRoute::None;
	bool bChangingActivity = false;

	// Resolved lazily from the VR char, re-resolved when the VR char changes.
	mutable TWeakObjectPtr<UWidgetInteractionComponent> CachedRightRay;
};
