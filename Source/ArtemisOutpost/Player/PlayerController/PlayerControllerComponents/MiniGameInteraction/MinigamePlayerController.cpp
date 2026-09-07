// Fill out your copyright notice in the Description page of Project Settings.

#include "ArtemisOutpost/Player/PlayerController/PlayerControllerComponents/MiniGameInteraction/MinigamePlayerController.h"
#include "ArtemisOutpost/Player/PlayerController/PawnController.h"
#include "ArtemisOutpost/MiniGames/UI/UIGame/MiniGameUI.h"
#include "ArtemisOutpost/MiniGames/MiniGameComponents/ConnectionComponent/ConnectionComponent.h"
#include "ArtemisOutpost/MiniGames/General/GameInstance/MinigameActor.h"
#include "ArtemisOutpost/MiniGames/General/GameInstance/CoupledAxisMinigameActor.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"

UMinigamePlayerController::UMinigamePlayerController()
{
	PrimaryComponentTick.bCanEverTick = false;
	// Must be replicated for the Server RPCs to route through the owning controller.
	SetIsReplicatedByDefault(true);
}

FString UMinigamePlayerController::GetPlayerUPID() const
{
	const APawnController* PC = Cast<APawnController>(GetOwner());
	return PC ? PC->GetPlayerUPID() : FString();
}

// ---- Transport ----

void UMinigamePlayerController::ServerRequestEnter_Implementation(AMinigameActor* Target)
{
	// First link in the chain. If you press "enter" and see NO [Enter] line at all, the Blueprint
	// never called ServerRequestEnter — the problem is the prompt / trigger wiring, not the game.
	if (!Target)
	{
		UE_LOG(LogMinigame, Error, TEXT("[Enter] REFUSED: Target minigame actor is null. The Blueprint passed no actor to ServerRequestEnter."));
		return;
	}

	UConnectionComponent* Connection = Target->GetConnectionComponent();
	if (!Connection)
	{
		UE_LOG(LogMinigame, Error, TEXT("[Enter] REFUSED: %s has no ConnectionComponent."), *Target->GetName());
		return;
	}

	UE_LOG(LogMinigame, Log, TEXT("[Enter] '%s' requests to enter %s (state=%s). Handing over to the connection."),
		*GetPlayerUPID(), *Target->GetName(), *UEnum::GetValueAsString(Target->GetState()));

	Connection->ServerRequestJoin(GetPlayerUPID());
}

void UMinigamePlayerController::ServerRequestLeave_Implementation(AMinigameActor* Target)
{
	if (Target)
	{
		if (UConnectionComponent* Connection = Target->GetConnectionComponent())
		{
			Connection->ServerRequestLeave(GetPlayerUPID());
		}
	}
}

void UMinigamePlayerController::ServerSubmitInput_Implementation(AMinigameActor* Target, FMinigameInput Input)
{
	if (Target)
	{
		Target->ServerHandleInput(GetPlayerUPID(), Input);
	}
}

// ---- View lifecycle ----

void UMinigamePlayerController::OpenUI(AMinigameActor* Target)
{
	if (ActiveView)
	{
		CloseUI();
	}
	if (!Target)
	{
		return;
	}

	ActiveTarget = Target;

	APlayerController* PC = Cast<APlayerController>(GetOwner());
	const TSubclassOf<UMiniGameUI> WidgetClass = Target->GetMiniGameUIClass();
	if (!PC || !WidgetClass)
	{
		return;
	}

	ActiveView = CreateWidget<UMiniGameUI>(PC, WidgetClass);
	if (!ActiveView)
	{
		return;
	}

	ActiveView->LocalUPID = GetPlayerUPID();
	if (const ACoupledAxisMinigameActor* Coupled = Cast<ACoupledAxisMinigameActor>(Target))
	{
		ActiveView->DwellSeconds = Coupled->GetDwellSeconds();
	}

	// View -> server.
	ActiveView->OnInput.AddDynamic(this, &UMinigamePlayerController::HandleUIInput);

	// Actor -> View. Bind the replicated update events, then push the current state once.
	Target->OnStateChanged.AddDynamic(this, &UMinigamePlayerController::HandleModelStateChanged);
	if (ACoupledAxisMinigameActor* Coupled = Cast<ACoupledAxisMinigameActor>(Target))
	{
		Coupled->OnAxesUpdated.AddDynamic(this, &UMinigamePlayerController::HandleModelAxesUpdated);
	}

	// World-space: hand the View to the VR pawn's holder in front of the HMD (+ dim), instead of a
	// screen-space AddToViewport (which does not render in the HMD). Falls back to the viewport if
	// there is no VR pawn (e.g. desktop/AR without the VR rig).
	ACharVR* VRPawn = nullptr;
	if (const APawnController* OwningPC = Cast<APawnController>(GetOwner()))
	{
		VRPawn = OwningPC->GetVRPawn();
	}
	if (VRPawn)
	{
		VRPawn->ShowMinigameView(ActiveView);
		bMiniGameActive = true; 
	}
	else
	{
		// No VR pawn resolved -> screen-space fallback (glued to the view, ignores the world-space
		// holder). If you see the HUD stuck to your face and moving the Minigame_View component does
		// nothing, THIS is why: GetVRPawn() returned null.
		UE_LOG(LogTemp, Warning, TEXT("[Minigame] OpenUI: GetVRPawn() is null -> screen-space fallback (world-space HUD holder not used)."));
		ActiveView->AddToViewport();
	}

	ActiveView->OnOpened();

	HandleModelStateChanged();
	if (const ACoupledAxisMinigameActor* Coupled = Cast<ACoupledAxisMinigameActor>(Target))
	{
		// May legitimately be empty here: Axes are filled on the server in OnStart and can arrive a
		// frame later on the client. The View rebuilds on the next OnAxesUpdated either way.
		ActiveView->PushAxes(Coupled->GetAxes());
	}
}

void UMinigamePlayerController::CloseUI()
{
	if (ActiveTarget)
	{
		ActiveTarget->OnStateChanged.RemoveDynamic(this, &UMinigamePlayerController::HandleModelStateChanged);
		if (ACoupledAxisMinigameActor* Coupled = Cast<ACoupledAxisMinigameActor>(ActiveTarget))
		{
			Coupled->OnAxesUpdated.RemoveDynamic(this, &UMinigamePlayerController::HandleModelAxesUpdated);
		}
	}

	if (ActiveView)
	{
		ActiveView->OnInput.RemoveDynamic(this, &UMinigamePlayerController::HandleUIInput);
		ActiveView->OnClosed();

		ACharVR* VRPawn = nullptr;
		if (const APawnController* OwningPC = Cast<APawnController>(GetOwner()))
		{
			VRPawn = OwningPC->GetVRPawn();
		}
		if (VRPawn)
		{
			VRPawn->HideMinigameView();
			bMiniGameActive = false; 
		}
		else
		{
			ActiveView->RemoveFromParent();
		}

		ActiveView = nullptr;
	}

	ActiveTarget = nullptr;
}

// ---- Wiring handlers ----

void UMinigamePlayerController::HandleUIInput(FMinigameInput Input)
{
	if (ActiveTarget)
	{
		ServerSubmitInput(ActiveTarget, Input);
	}
}

void UMinigamePlayerController::HandleModelStateChanged()
{
	if (ActiveView && ActiveTarget)
	{
		ActiveView->PushState(ActiveTarget->GetState());
	}
}

void UMinigamePlayerController::HandleModelAxesUpdated(const TArray<FAxisData>& Axes)
{
	if (ActiveView)
	{
		ActiveView->PushAxes(Axes);
	}
}

bool UMinigamePlayerController::IsMiniGameActive()
{
	return bMiniGameActive;
}

void UMinigamePlayerController::SubmitInputAction(UInputAction* InputAction, EInputActionType InputActionType)
{
	// While the View shows a menu screen it owns the thumbstick (highlight navigation), so ONE stick
	// action drives both the axis-selection screen and the rotation gameplay. Withhold it from the
	// actor in that case — otherwise the rotate handler would run against a not-yet-claimed axis and
	// silently do nothing. Note this withholds EVERY action of the minigame context while the menu is
	// up; that is intended (nothing else is meaningful before an axis is claimed).
	if (ActiveView && ActiveView->WantsNavigationInput())
	{
		if (InputActionType == EInputActionType::Triggered)
		{
			ActiveView->HandleNavigate(GetLocalActionValue(InputAction));
		}
		return;
	}

	if (!ActiveTarget)
	{
		return;
	}

	ActiveTarget->ProcessInput(InputAction, InputActionType);
}

void UMinigamePlayerController::SubmitEnterAction()
{
	if (!ActiveView)
	{
		return;
	}

	switch (ActiveView->ConfirmHighlighted())
	{
	case EMiniGameUIAction::RequestLeave:
		// The View decided the press means "I am done". Leaving is server-authoritative: the
		// connection frees the slot, the actor releases this player's axis (values are kept), and
		// once the last participant is gone the actor judges the result. The View closes on its own,
		// because the replicated slot change runs AMinigameActor::RefreshLocalUI -> CloseUI.
		if (ActiveTarget)
		{
			ServerRequestLeave(ActiveTarget);
		}
		break;

	case EMiniGameUIAction::Handled:
	case EMiniGameUIAction::None:
	default:
		break;
	}
}

FVector2D UMinigamePlayerController::GetLocalActionValue(const UInputAction* Action) const
{
	if (!Action)
	{
		return FVector2D::ZeroVector;
	}

	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	ULocalPlayer* LP = PC ? PC->GetLocalPlayer() : nullptr;
	if (!LP)
	{
		return FVector2D::ZeroVector;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	UEnhancedPlayerInput* PlayerInput = Subsystem ? Subsystem->GetPlayerInput() : nullptr;

	return PlayerInput ? PlayerInput->GetActionValue(Action).Get<FVector2D>() : FVector2D::ZeroVector;
}
