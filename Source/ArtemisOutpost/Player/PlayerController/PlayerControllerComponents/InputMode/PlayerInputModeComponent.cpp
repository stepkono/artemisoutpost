// Fill out your copyright notice in the Description page of Project Settings.

#include "PlayerInputModeComponent.h"

#include "ArtemisOutpost/MiniGames/General/MinigameTypes.h"
#include "ArtemisOutpost/Player/PawnVR/ACharVR.h"
#include "ArtemisOutpost/Player/PawnVR/HandTools/HandToolBase.h"
#include "ArtemisOutpost/Player/PawnVR/ToolsHUD/ToolsHUDComponent.h"
#include "ArtemisOutpost/Player/PlayerController/PawnController.h"
#include "ArtemisOutpost/Player/PlayerController/PlayerControllerComponents/MiniGameInteraction/MinigamePlayerController.h"
#include "Components/WidgetComponent.h"
#include "Components/WidgetInteractionComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

namespace
{
	FString ActivityName(EInputActivity Activity)
	{
		return StaticEnum<EInputActivity>()->GetNameStringByValue(static_cast<int64>(Activity));
	}
}

UPlayerInputModeComponent::UPlayerInputModeComponent()
{
	// Event driven only. Client-local, so not replicated.
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

UPlayerInputModeComponent* UPlayerInputModeComponent::FindFor(const AActor* Actor)
{
	// Walk up the owner chain (tool -> pawn, HUD component owner -> pawn) to the first pawn or controller.
	const AActor* Current = Actor;
	for (int32 Depth = 0; Current && Depth < 4; ++Depth)
	{
		const AController* Controller = Cast<AController>(Current);
		if (!Controller)
		{
			if (const APawn* Pawn = Cast<APawn>(Current))
			{
				Controller = Pawn->GetController();
				if (!Controller)
				{
					return nullptr; // remote proxy or not possessed
				}
			}
		}
		if (Controller)
		{
			return Controller->FindComponentByClass<UPlayerInputModeComponent>();
		}
		Current = Current->GetOwner() ? Current->GetOwner() : Current->GetParentActor();
	}
	return nullptr;
}

// ---- Input ----

void UPlayerInputModeComponent::BindInput(UEnhancedInputComponent* EnhancedInputComponent)
{
	if (!EnhancedInputComponent)
	{
		return;
	}
	if (!TriggerAction)
	{
		UE_LOG(LogMinigame, Error, TEXT("[InputMode] TriggerAction is not set on the InputMode component (BP_PawnController) -> the right trigger does nothing."));
		return;
	}

	EnhancedInputComponent->BindAction(TriggerAction, ETriggerEvent::Started, this, &UPlayerInputModeComponent::HandleTriggerPressed);
	EnhancedInputComponent->BindAction(TriggerAction, ETriggerEvent::Completed, this, &UPlayerInputModeComponent::HandleTriggerReleased);
	EnhancedInputComponent->BindAction(TriggerAction, ETriggerEvent::Canceled, this, &UPlayerInputModeComponent::HandleTriggerReleased);
}

void UPlayerInputModeComponent::HandleTrigger(bool bPressed)
{
	if (!IsLocal())
	{
		return;
	}

	ACharVR* VR = GetVRChar();

	if (bPressed)
	{
		// 1. Hover always wins, in every activity.
		if (IsHoveringWorldWidget())
		{
			PressedRoute = ETriggerRoute::WorldWidget;
			UE_LOG(LogMinigame, Verbose, TEXT("[InputMode] trigger press -> world widget (activity %s)."), *ActivityName(Activity));
			GetRightRay()->PressPointerKey(EKeys::LeftMouseButton);
			return;
		}

		switch (Activity)
		{
		case EInputActivity::HUDMenu:
			PressedRoute = ETriggerRoute::HUD;
			if (UToolsHUDComponent* HUD = VR ? VR->FindComponentByClass<UToolsHUDComponent>() : nullptr)
			{
				HUD->ConfirmSelection();
			}
			break;

		case EInputActivity::MiniGame:
			PressedRoute = ETriggerRoute::MiniGame;
			if (const APawnController* PC = Cast<APawnController>(GetOwner()))
			{
				if (UMinigamePlayerController* MGC = PC->GetMinigamePlayerController())
				{
					MGC->SubmitEnterAction();
				}
			}
			break;

		case EInputActivity::HandTool:
			PressedRoute = ETriggerRoute::HandTool;
			if (AHandToolBase* Tool = VR ? VR->GetActiveTool() : nullptr)
			{
				Tool->ExecuteAction();
			}
			else
			{
				UE_LOG(LogMinigame, Warning, TEXT("[InputMode] trigger press in HandTool but the pawn has no active tool."));
			}
			break;

		case EInputActivity::Free:
		default:
			PressedRoute = ETriggerRoute::None;
			break;
		}
		return;
	}

	// Release: whatever handled the press gets the release.
	switch (PressedRoute)
	{
	case ETriggerRoute::WorldWidget:
		if (UWidgetInteractionComponent* Ray = GetRightRay())
		{
			Ray->ReleasePointerKey(EKeys::LeftMouseButton);
		}
		break;

	case ETriggerRoute::HandTool:
		// The tool may have been holstered meanwhile (it then stopped itself in DeactivateTool).
		if (AHandToolBase* Tool = VR ? VR->GetActiveTool() : nullptr)
		{
			Tool->EndAction();
		}
		break;

	default:
		break;
	}
	PressedRoute = ETriggerRoute::None;
}

bool UPlayerInputModeComponent::IsHoveringWorldWidget() const
{
	// A ray suppressed via UControllerRayComponent::SetRayEnabled is inactive and its hover state stale.
	const UWidgetInteractionComponent* Ray = GetRightRay();
	if (!Ray || !Ray->IsActive() || !Ray->IsOverInteractableWidget())
	{
		return false;
	}

	// The HUD anchor and the minigame View holder are WidgetComponents on the own pawn. Their input
	// comes from the HUD / minigame paths, a ray hit on them must not steal the trigger.
	const UWidgetComponent* Hovered = Ray->GetHoveredWidgetComponent();
	const AActor* HoveredOwner = Hovered ? Hovered->GetOwner() : nullptr;
	return HoveredOwner && HoveredOwner != GetVRChar();
}

// ---- Activity ----

bool UPlayerInputModeComponent::CanEnter(EInputActivity NewActivity) const
{
	// D1: the HUD cannot be opened while inside a minigame. Leaving is explicit (Cancel / the View).
	if (NewActivity == EInputActivity::HUDMenu && Activity == EInputActivity::MiniGame)
	{
		return false;
	}
	return true;
}

void UPlayerInputModeComponent::SetActivity(EInputActivity NewActivity, const FString& Reason)
{
	if (!IsLocal())
	{
		return;
	}
	if (bChangingActivity)
	{
		// A teardown called back into us (CloseMenu, DeactivateTool). The outer transition decides.
		UE_LOG(LogMinigame, Verbose, TEXT("[InputMode] nested SetActivity(%s, %s) during a transition -> ignored."),
			*ActivityName(NewActivity), *Reason);
		return;
	}
	if (NewActivity == Activity)
	{
		return;
	}
	if (!CanEnter(NewActivity))
	{
		UE_LOG(LogMinigame, Log, TEXT("[InputMode] %s -> %s REFUSED (%s)."), *ActivityName(Activity), *ActivityName(NewActivity), *Reason);
		return;
	}

	const EInputActivity OldActivity = Activity;
	{
		TGuardValue<bool> Guard(bChangingActivity, true);
		TearDown(OldActivity);
	}

	Activity = NewActivity;
	ApplyContexts();

	UE_LOG(LogMinigame, Log, TEXT("[InputMode] %s -> %s (%s). Contexts: %s"),
		*ActivityName(OldActivity), *ActivityName(NewActivity), *Reason, *DescribeContexts());

	OnActivityChanged.Broadcast(OldActivity, NewActivity);
}

void UPlayerInputModeComponent::LeaveActivity(EInputActivity Expected, const FString& Reason)
{
	if (Activity == Expected)
	{
		SetActivity(EInputActivity::Free, Reason);
	}
}

void UPlayerInputModeComponent::EnterHandTool(AHandToolBase* Tool)
{
	if (!IsLocal() || !Tool)
	{
		return;
	}
	if (bChangingActivity)
	{
		UE_LOG(LogMinigame, Warning, TEXT("[InputMode] %s drawn during a transition -> activity not changed."), *Tool->GetName());
		return;
	}

	// Another tool is still drawn: holster it without leaving HandTool (guard swallows its report).
	ACharVR* VR = GetVRChar();
	AHandToolBase* Current = VR ? VR->GetActiveTool() : nullptr;
	if (Current && Current != Tool)
	{
		UE_LOG(LogMinigame, Log, TEXT("[InputMode] HandTool: switching %s -> %s."), *Current->GetName(), *Tool->GetName());
		TGuardValue<bool> Guard(bChangingActivity, true);
		Current->DeactivateTool();
	}

	SetActivity(EInputActivity::HandTool, Tool->GetName());
}

void UPlayerInputModeComponent::TearDown(EInputActivity Leaving)
{
	ACharVR* VR = GetVRChar();

	switch (Leaving)
	{
	case EInputActivity::HUDMenu:
		if (UToolsHUDComponent* HUD = VR ? VR->FindComponentByClass<UToolsHUDComponent>() : nullptr)
		{
			if (HUD->IsMenuOpen())
			{
				HUD->CloseMenu();
			}
		}
		break;

	case EInputActivity::HandTool:
		// Hides the mesh, stops the tick, ends a placement / scan (tool overrides).
		if (AHandToolBase* Tool = VR ? VR->GetActiveTool() : nullptr)
		{
			Tool->DeactivateTool();
		}
		break;

	case EInputActivity::MiniGame:
		// Normally the View closed through the replicated slot change and there is nothing left. If the
		// activity is left for another reason while a slot is still held, give the slot back. The View
		// then closes on its own once the change replicates.
		if (const APawnController* PC = Cast<APawnController>(GetOwner()))
		{
			UMinigamePlayerController* MGC = PC->GetMinigamePlayerController();
			if (MGC && MGC->HasActiveMinigame())
			{
				MGC->RequestLeave();
			}
		}
		break;

	case EInputActivity::Free:
	default:
		break;
	}
}

void UPlayerInputModeComponent::ApplyContexts() const
{
	UEnhancedInputLocalPlayerSubsystem* Subsystem = GetInputSubsystem();
	if (!Subsystem)
	{
		UE_LOG(LogMinigame, Warning, TEXT("[InputMode] no EnhancedInput local player subsystem -> contexts not changed."));
		return;
	}

	auto Sync = [Subsystem](UInputMappingContext* Context, bool bWanted, int32 Priority)
	{
		if (!Context)
		{
			return;
		}
		const bool bPresent = Subsystem->HasMappingContext(Context);
		if (bWanted && !bPresent)
		{
			Subsystem->AddMappingContext(Context, Priority);
		}
		else if (!bWanted && bPresent)
		{
			Subsystem->RemoveMappingContext(Context);
		}
	};

	Sync(HUDContext, Activity == EInputActivity::HUDMenu, HUDContextPriority);
	Sync(MiniGameContext, Activity == EInputActivity::MiniGame, MiniGameContextPriority);
}

// ---- Helpers ----

bool UPlayerInputModeComponent::IsLocal() const
{
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	return PC && PC->IsLocalController();
}

ACharVR* UPlayerInputModeComponent::GetVRChar() const
{
	// The VR char reference survives an AR switch, so a teardown after the switch still reaches its HUD
	// and tools.
	const APawnController* PC = Cast<APawnController>(GetOwner());
	return PC ? PC->GetVRPawn() : nullptr;
}

UWidgetInteractionComponent* UPlayerInputModeComponent::GetRightRay() const
{
	ACharVR* VR = GetVRChar();
	if (!VR)
	{
		return nullptr;
	}
	if (CachedRightRay.IsValid() && CachedRightRay->GetOwner() == VR)
	{
		return CachedRightRay.Get();
	}

	TArray<UWidgetInteractionComponent*> Rays;
	VR->GetComponents<UWidgetInteractionComponent>(Rays);
	for (UWidgetInteractionComponent* Ray : Rays)
	{
		if (Ray && Ray->ComponentHasTag(RightRayTag))
		{
			CachedRightRay = Ray;
			return Ray;
		}
	}

	UE_LOG(LogMinigame, Warning, TEXT("[InputMode] no WidgetInteractionComponent tagged '%s' on %s -> world widgets cannot be pressed."),
		*RightRayTag.ToString(), *VR->GetName());
	return nullptr;
}

UEnhancedInputLocalPlayerSubsystem* UPlayerInputModeComponent::GetInputSubsystem() const
{
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	const ULocalPlayer* LP = PC ? PC->GetLocalPlayer() : nullptr;
	return LP ? LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
}

FString UPlayerInputModeComponent::DescribeContexts() const
{
	const UEnhancedInputLocalPlayerSubsystem* Subsystem = GetInputSubsystem();
	if (!Subsystem)
	{
		return TEXT("n/a");
	}
	auto State = [Subsystem](const UInputMappingContext* Context) -> const TCHAR*
	{
		if (!Context)
		{
			return TEXT("unset");
		}
		return Subsystem->HasMappingContext(Context) ? TEXT("on") : TEXT("off");
	};
	return FString::Printf(TEXT("HUD=%s MG=%s"), State(HUDContext), State(MiniGameContext));
}
