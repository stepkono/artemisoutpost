// Fill out your copyright notice in the Description page of Project Settings.

#include "ArtemisOutpost/Player/MiniGameInteraction/MinigamePlayerController.h"
#include "ArtemisOutpost/Player/PawnController.h"
#include "ArtemisOutpost/Minigame/UI/Game/MiniGameUI.h"
#include "ArtemisOutpost/Minigame/Connection/ConnectionComponent.h"
#include "ArtemisOutpost/Minigame/General/Logic/CoupledAxisLogicComponent.h"
#include "ArtemisOutpost/Minigame/General/GameInstance/MinigameActor.h"

UMinigamePlayerController::UMinigamePlayerController()
{
	PrimaryComponentTick.bCanEverTick = false;
	// Must be replicated for the Server RPCs to route through the owning controller.
	SetIsReplicatedByDefault(true);
}

FString UMinigamePlayerController::GetOwnerUPID() const
{
	const APawnController* PC = Cast<APawnController>(GetOwner());
	return PC ? PC->GetPlayerUPID() : FString();
}

// ---- Transport ----

void UMinigamePlayerController::ServerRequestEnter_Implementation(AMinigameActor* Target)
{
	if (Target)
	{
		if (UConnectionComponent* Connection = Target->GetConnectionComponent())
		{
			Connection->ServerRequestJoin(GetOwnerUPID());
		}
	}
}

void UMinigamePlayerController::ServerRequestLeave_Implementation(AMinigameActor* Target)
{
	if (Target)
	{
		if (UConnectionComponent* Connection = Target->GetConnectionComponent())
		{
			Connection->ServerRequestLeave(GetOwnerUPID());
		}
	}
}

void UMinigamePlayerController::ServerSubmitInput_Implementation(AMinigameActor* Target, FMinigameInput Input)
{
	if (Target)
	{
		if (UMinigameLogicComponent* Logic = Target->GetLogicComponent())
		{
			Logic->ServerHandleInput(GetOwnerUPID(), Input);
		}
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
	ActiveModel = Target->GetLogicComponent();

	APlayerController* PC = Cast<APlayerController>(GetOwner());
	const TSubclassOf<UMiniGameUI> WidgetClass = ActiveModel ? ActiveModel->GetMiniGameUIClass() : nullptr;
	if (!PC || !WidgetClass)
	{
		return;
	}

	ActiveView = CreateWidget<UMiniGameUI>(PC, WidgetClass);
	if (!ActiveView)
	{
		return;
	}

	ActiveView->LocalUPID = GetOwnerUPID();
	if (const UCoupledAxisLogicComponent* Coupled = Cast<UCoupledAxisLogicComponent>(ActiveModel))
	{
		ActiveView->DwellSeconds = Coupled->GetDwellSeconds();
	}

	// View -> server.
	ActiveView->OnInput.AddDynamic(this, &UMinigamePlayerController::HandleUIInput);

	// Model -> View. Bind the replicated update events, then push the current state once.
	ActiveModel->OnStateChanged.AddDynamic(this, &UMinigamePlayerController::HandleModelStateChanged);
	if (UCoupledAxisLogicComponent* Coupled = Cast<UCoupledAxisLogicComponent>(ActiveModel))
	{
		Coupled->OnAxesUpdated.AddDynamic(this, &UMinigamePlayerController::HandleModelAxesUpdated);
	}

	ActiveView->AddToViewport();
	ActiveView->OnOpened();

	HandleModelStateChanged();
	if (const UCoupledAxisLogicComponent* Coupled = Cast<UCoupledAxisLogicComponent>(ActiveModel))
	{
		ActiveView->OnAxesUpdated(Coupled->GetAxes());
	}
}

void UMinigamePlayerController::CloseUI()
{
	if (ActiveModel)
	{
		ActiveModel->OnStateChanged.RemoveDynamic(this, &UMinigamePlayerController::HandleModelStateChanged);
		if (UCoupledAxisLogicComponent* Coupled = Cast<UCoupledAxisLogicComponent>(ActiveModel))
		{
			Coupled->OnAxesUpdated.RemoveDynamic(this, &UMinigamePlayerController::HandleModelAxesUpdated);
		}
	}

	if (ActiveView)
	{
		ActiveView->OnInput.RemoveDynamic(this, &UMinigamePlayerController::HandleUIInput);
		ActiveView->OnClosed();
		ActiveView->RemoveFromParent();
		ActiveView = nullptr;
	}

	ActiveModel = nullptr;
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
	if (ActiveView && ActiveModel)
	{
		ActiveView->OnMinigameStateChanged(ActiveModel->GetState());
	}
}

void UMinigamePlayerController::HandleModelAxesUpdated(const TArray<FAxisState>& Axes)
{
	if (ActiveView)
	{
		ActiveView->OnAxesUpdated(Axes);
	}
}
