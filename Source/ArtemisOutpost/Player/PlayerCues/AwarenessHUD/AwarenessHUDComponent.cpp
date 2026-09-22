// Fill out your copyright notice in the Description page of Project Settings.

#include "AwarenessHUDComponent.h"

#include "AwarenessHUDWidget.h"
#include "Components/WidgetComponent.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "ArtemisOutpost/GameData/ArtemisPlayerState.h"
#include "ArtemisOutpost/Networking/ClientServerConnection/NetUtils.h"
#include "ArtemisOutpost/Player/PlayerController/PawnController.h"
#include "ArtemisOutpost/Player/PlayerCues/PlayerCuesManager.h"

UAwarenessHUDComponent::UAwarenessHUDComponent()
{
	// Ticks for the lazy input binding and the refresh timer while open. Client-local.
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(false);
}

void UAwarenessHUDComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bIsOpen)
	{
		Close();
	}
	if (AnchorWidgetComp)
	{
		AnchorWidgetComp->SetWidget(nullptr);
	}
	ActiveWidget = nullptr;
	AnchorWidgetComp = nullptr;

	Super::EndPlay(EndPlayReason);
}

void UAwarenessHUDComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (ArtemisNet::IsServerHost(GetNetMode()))
	{
		return; // the host has no player looking at anything
	}

	if (!bIsOpen)
	{
		return;
	}

	// The player switched context while holding: the other pawn's HUD takes over on its next press.
	if (!IsLocalActivePawn())
	{
		Close();
		return;
	}

	RefreshAccum += DeltaTime;
	if (RefreshAccum >= RefreshInterval)
	{
		RefreshAccum = 0.0f;
		Refresh();
	}
}

// ---- Identity helpers ----

APawnController* UAwarenessHUDComponent::GetLocalController() const
{
	return Cast<APawnController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
}

UPlayerCuesManager* UAwarenessHUDComponent::GetCuesManager() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UPlayerCuesManager>() : nullptr;
}

bool UAwarenessHUDComponent::IsLocalActivePawn() const
{
	if (const UPlayerCuesManager* Cues = GetCuesManager())
	{
		return Cues->IsLocalActivePawn();
	}
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return Pawn && Pawn->IsLocallyControlled();
}

EPlayerContext UAwarenessHUDComponent::GetLocalContext() const
{
	// Prefer the replicated truth; before it arrives, derive from the local XR mode.
	if (const APawnController* PC = GetLocalController())
	{
		if (const AArtemisPlayerState* PS = PC->GetArtemisPlayerState())
		{
			return PS->GetCueState().Context;
		}
		return PC->GetXRMode() == EXRMode::VR ? EPlayerContext::VR : EPlayerContext::AR;
	}
	return EPlayerContext::AR;
}

// ---- Input ----

void UAwarenessHUDComponent::BindInput(UEnhancedInputComponent* EIC)
{
	if (!EIC || !HoldAction)
	{
		return;
	}
	EIC->BindAction(HoldAction, ETriggerEvent::Started,   this, &UAwarenessHUDComponent::HandleHoldStarted);
	EIC->BindAction(HoldAction, ETriggerEvent::Completed, this, &UAwarenessHUDComponent::HandleHoldCompleted);
}

void UAwarenessHUDComponent::HandleHoldStarted()
{
	if (IsLocalActivePawn())
	{
		Open();
	}
}

void UAwarenessHUDComponent::HandleHoldCompleted()
{
	Close();
}

// ---- Open / close ----

void UAwarenessHUDComponent::Open()
{
	if (bIsOpen)
	{
		return;
	}
	if (!EnsureWidget())
	{
		UE_LOG(LogTemp, Error, TEXT("[AwarenessHUD] Open aborted on %s: widget setup incomplete (WidgetClass / anchor tag '%s')."),
			*GetNameSafe(GetOwner()), *WidgetAnchorTag.ToString());
		return;
	}

	bIsOpen = true;
	RefreshAccum = 0.0f;
	Refresh();

	AnchorWidgetComp->SetVisibility(true, true);
	ActiveWidget->NotifyOpened();
}

void UAwarenessHUDComponent::Close()
{
	if (!bIsOpen)
	{
		return;
	}
	bIsOpen = false;

	if (ActiveWidget)
	{
		ActiveWidget->NotifyClosed();
	}
	if (AnchorWidgetComp)
	{
		AnchorWidgetComp->SetVisibility(false, true);
	}
}

// ---- Data ----

void UAwarenessHUDComponent::Refresh()
{
	if (!ActiveWidget)
	{
		return;
	}

	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	const APawnController* PC = GetLocalController();
	const FString LocalUPID = PC ? PC->GetPlayerUPID() : FString();

	TArray<FAwarenessHUDRow> Rows;
	if (GS)
	{
		for (const APlayerState* PS : GS->PlayerArray)
		{
			const AArtemisPlayerState* APS = Cast<AArtemisPlayerState>(PS);
			if (!APS || APS->GetUPID().IsEmpty())
			{
				continue; // not one of ours, or identity not replicated yet
			}

			const FPlayerCueState& State = APS->GetCueState();

			FAwarenessHUDRow Row;
			Row.UPID           = APS->GetUPID();
			Row.PlayerNumber   = APS->GetPlayerNumber();
			Row.Context        = State.Context;
			Row.Activity       = State.GetActivity();
			Row.ActivityText   = UAwarenessHUDWidget::MakeActivityText(State);
			Row.bTalking       = State.bTalking;
			Row.bIsLocalPlayer = (Row.UPID == LocalUPID);
			Row.CueState       = State;
			Rows.Add(MoveTemp(Row));
		}
	}

	ActiveWidget->SetRows(Rows, GetLocalContext());
}

// ---- Setup ----

bool UAwarenessHUDComponent::EnsureWidget()
{
	if (ActiveWidget && AnchorWidgetComp)
	{
		return true;
	}

	if (!WidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[AwarenessHUD] WidgetClass is not set on %s."), *GetNameSafe(GetOwner()));
		return false;
	}

	APawnController* PC = GetLocalController();
	if (!PC)
	{
		return false;
	}

	AnchorWidgetComp = ResolveAnchor();
	if (!AnchorWidgetComp)
	{
		UE_LOG(LogTemp, Error, TEXT("[AwarenessHUD] No WidgetComponent tagged '%s' on %s."), *WidgetAnchorTag.ToString(), *GetNameSafe(GetOwner()));
		return false;
	}

	if (!ActiveWidget)
	{
		ActiveWidget = CreateWidget<UAwarenessHUDWidget>(PC, WidgetClass);
		if (!ActiveWidget)
		{
			UE_LOG(LogTemp, Error, TEXT("[AwarenessHUD] CreateWidget failed on %s."), *GetNameSafe(GetOwner()));
			return false;
		}
	}

	AnchorWidgetComp->SetWidget(ActiveWidget);
	AnchorWidgetComp->SetVisibility(false, true);
	return true;
}

UWidgetComponent* UAwarenessHUDComponent::ResolveAnchor() const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	TArray<UWidgetComponent*> WidgetComps;
	Owner->GetComponents<UWidgetComponent>(WidgetComps);
	for (UWidgetComponent* WC : WidgetComps)
	{
		if (WC && WC->ComponentHasTag(WidgetAnchorTag))
		{
			return WC;
		}
	}
	return nullptr;
}
