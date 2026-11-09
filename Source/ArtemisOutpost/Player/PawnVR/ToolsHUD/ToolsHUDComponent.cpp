// Fill out your copyright notice in the Description page of Project Settings.

#include "ToolsHUDComponent.h"

#include "ToolsHUDWidget.h"
#include "ArtemisOutpost/Player/PlayerController/PlayerControllerComponents/InputMode/PlayerInputModeComponent.h"
#include "Components/WidgetComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#define LOCTEXT_NAMESPACE "ToolsHUD"

UToolsHUDComponent::UToolsHUDComponent()
{
	// Pure UI orchestrator — no tick, no replication (client-local).
	PrimaryComponentTick.bCanEverTick = false;
}

void UToolsHUDComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UToolsHUDComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bIsOpen)
	{
		CloseMenu();
	}

	if (ActiveWidget)
	{
		ActiveWidget->AvailabilityDelegate.Unbind();
	}
	if (AnchorWidgetComp)
	{
		AnchorWidgetComp->SetWidget(nullptr);
	}
	ActiveWidget = nullptr;
	AnchorWidgetComp = nullptr;

	Super::EndPlay(EndPlayReason);
}

// ---- Input ----

void UToolsHUDComponent::BindInput(UEnhancedInputComponent* EnhancedInputComponent)
{
	if (!EnhancedInputComponent)
	{
		return;
	}

	// Only the thumbstick navigation is bound in C++ (fires continuously; debounced in the widget).
	// The trigger is routed in Blueprint and calls ConfirmSelection() while the menu is open.
	if (NavigateAction)
	{
		EnhancedInputComponent->BindAction(NavigateAction, ETriggerEvent::Triggered, this, &UToolsHUDComponent::HandleNavigateInput);
	}
}

void UToolsHUDComponent::HandleNavigateInput(const FInputActionValue& Value)
{
	if (bIsOpen && ActiveWidget)
	{
		ActiveWidget->HandleNavigate(Value.Get<FVector2D>());
	}
}

void UToolsHUDComponent::ConfirmSelection()
{
	if (!bIsOpen || !ActiveWidget)
	{
		return;
	}

	EHUDAction Action;
	if (ActiveWidget->ConfirmHighlighted(Action))
	{
		HandleToolActionConfirmed(Action);
	}
}

// ---- Open / close (focus) ----

void UToolsHUDComponent::OpenMenu()
{
	if (bIsOpen)
	{
		return;
	}

	// The input-mode owner decides whether the HUD may open (not inside a minigame).
	UPlayerInputModeComponent* InputMode = UPlayerInputModeComponent::FindFor(GetOwner());
	if (InputMode && !InputMode->CanEnter(EInputActivity::HUDMenu))
	{
		UE_LOG(LogTemp, Log, TEXT("[ToolsHUD] OpenMenu refused: the player is inside a minigame."));
		return;
	}

	if (!EnsureWidget())
	{
		UE_LOG(LogTemp, Error, TEXT("[ToolsHUD] OpenMenu aborted: widget setup incomplete (WidgetClass / anchor)."));
		return;
	}

	if (AnchorWidgetComp)
	{
		AnchorWidgetComp->SetVisibility(true);
	}

	ActiveWidget->OpenToPage(EToolHUDPage::Main);
	bIsOpen = true;

	// Focus: the owner adds IMC_HUD and holsters a drawn tool.
	if (InputMode)
	{
		InputMode->SetActivity(EInputActivity::HUDMenu, TEXT("HUD opened"));
	}
}

void UToolsHUDComponent::CloseMenu()
{
	if (!bIsOpen)
	{
		return;
	}

	if (ActiveWidget)
	{
		ActiveWidget->NotifyClosed();
	}
	if (AnchorWidgetComp)
	{
		AnchorWidgetComp->SetVisibility(false);
	}

	bIsOpen = false;

	// Every close path (X tile, Build*/Scan* tile, toggle, teardown) reports here, so IMC_HUD is always
	// removed. A Build*/Scan* tile then draws its tool, which enters HandTool.
	if (UPlayerInputModeComponent* InputMode = UPlayerInputModeComponent::FindFor(GetOwner()))
	{
		InputMode->LeaveActivity(EInputActivity::HUDMenu, TEXT("HUD closed"));
	}
}

void UToolsHUDComponent::ToggleMenu()
{
	bIsOpen ? CloseMenu() : OpenMenu();
}

void UToolsHUDComponent::RefreshGating()
{
	if (bIsOpen && ActiveWidget)
	{
		ActiveWidget->RefreshGating();
	}
}

// ---- Dispatch (widget -> component) ----

void UToolsHUDComponent::HandleToolActionConfirmed(EHUDAction Action)
{
	// Internal navigation / close — the component owns page state and menu visibility.
	switch (Action)
	{
	case EHUDAction::OpenBuilding:
		if (ActiveWidget) { ActiveWidget->GoToPage(EToolHUDPage::Building); }
		break;

	case EHUDAction::OpenScanning:
		if (ActiveWidget) { ActiveWidget->GoToPage(EToolHUDPage::Scanning); }
		break;

	case EHUDAction::Back:
		if (ActiveWidget) { ActiveWidget->GoToPage(EToolHUDPage::Main); }
		break;

	// Close on the X tile and on any gameplay action (spec: choosing a build/scan closes the HUD).
	case EHUDAction::CloseMenu:
	case EHUDAction::BuildHabitat:
	case EHUDAction::BuildSolarPanel:
	case EHUDAction::BuildAntenna:
	case EHUDAction::SurfaceScan:
	case EHUDAction::ResourceMiner:
	case EHUDAction::AreaScan:
		CloseMenu();
		break;

	default:
		break;
	}

	// One public event carrying the raw action. BP switches on it for the leaf actions only:
	// Build* -> BeginPlacement, SurfaceScan/ResourceMiner -> BeginScan, AreaScan -> its tool. Those
	// draw the tool themselves. Page actions (OpenBuilding/OpenScanning/Back) must NOT draw a tool:
	// drawing enters HandTool, which tears the HUD down before the sub-page is visible.
	OnHUDAction.Broadcast(Action);
}

// ---- Gating ----

bool UToolsHUDComponent::QueryAvailability(EHUDAction Action, FText& OutReason)
{
	// Debug override wins (bring-up: verify greying-out without a resource system).
	if (const bool* Override = DebugAvailabilityOverride.Find(Action))
	{
		if (!*Override)
		{
			OutReason = LOCTEXT("Locked", "Voraussetzungen nicht erfüllt");
		}
		return *Override;
	}

	return IsActionAvailable(Action, OutReason);
}

bool UToolsHUDComponent::IsActionAvailable_Implementation(EHUDAction Action, FText& OutReason)
{
	// No resource system yet — everything is available. Override in a BP child of this component, or
	// replace this body, to gate on real replicated resources (e.g. regolith for BuildHabitat).
	OutReason = FText::GetEmpty();
	return true;
}

// ---- Setup helpers ----

bool UToolsHUDComponent::EnsureWidget()
{
	if (ActiveWidget)
	{
		return true;
	}

	if (!WidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[ToolsHUD] WidgetClass is not set on the ToolsHUDComponent."));
		return false;
	}

	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC)
	{
		UE_LOG(LogTemp, Error, TEXT("[ToolsHUD] EnsureWidget: owner has no local PlayerController."));
		return false;
	}

	AnchorWidgetComp = ResolveAnchor();
	if (!AnchorWidgetComp)
	{
		UE_LOG(LogTemp, Error, TEXT("[ToolsHUD] EnsureWidget: no WidgetComponent tagged '%s' on the pawn."),
			*WidgetAnchorTag.ToString());
		return false;
	}

	ActiveWidget = CreateWidget<UToolsHUDWidget>(PC, WidgetClass);
	if (!ActiveWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("[ToolsHUD] EnsureWidget: CreateWidget failed."));
		return false;
	}

	ActiveWidget->AvailabilityDelegate.BindUObject(this, &UToolsHUDComponent::QueryAvailability);

	AnchorWidgetComp->SetWidget(ActiveWidget);
	AnchorWidgetComp->SetVisibility(false);

	return true;
}

UWidgetComponent* UToolsHUDComponent::ResolveAnchor() const
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

	// Convenience fallback: exactly one WidgetComponent and it's untagged — use it, but warn so the
	// tag gets added (important once the pawn has more than one widget).
	if (WidgetComps.Num() == 1 && WidgetComps[0])
	{
		UE_LOG(LogTemp, Warning, TEXT("[ToolsHUD] No WidgetComponent tagged '%s'; falling back to the only one present. Tag it to be safe."),
			*WidgetAnchorTag.ToString());
		return WidgetComps[0];
	}

	return nullptr;
}

#undef LOCTEXT_NAMESPACE
