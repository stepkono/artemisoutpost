// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ToolsHUDTypes.generated.h"

class UTexture2D;

// The pages of the wrist Tools-HUD. The HUD is a single widget with a WidgetSwitcher; this enum
// selects which page the switcher shows. Purely client-local UI state (never replicated).
UENUM(BlueprintType)
enum class EToolHUDPage : uint8
{
	Main      UMETA(DisplayName = "Main"),
	Building  UMETA(DisplayName = "Building"),
	Scanning  UMETA(DisplayName = "Scanning")
};

// Every selectable intent the HUD can emit. The value both identifies a tile AND tells the owning
// component what to do on confirm (navigate to a sub-page, close, or dispatch a gameplay action).
// Grouped by page for readability; the page->tile mapping lives in UToolsHUDWidget::GetActionsForPage.
UENUM(BlueprintType)
enum class EHUDAction : uint8
{
	None            UMETA(DisplayName = "None"),

	// --- Main page ---
	OpenBuilding    UMETA(DisplayName = "Open Building Mode"),
	OpenScanning    UMETA(DisplayName = "Open Scanning Mode"),
	CloseMenu       UMETA(DisplayName = "Close Menu"),

	// --- Building page ---
	BuildHabitat    UMETA(DisplayName = "Build Habitat"),
	BuildSolarPanel UMETA(DisplayName = "Build Solar Panel"),
	BuildAntenna    UMETA(DisplayName = "Build Antenna"),

	// --- Scanning page ---
	SurfaceScan     UMETA(DisplayName = "Surface Scan"),
	AreaScan        UMETA(DisplayName = "Area Scan"),

	// --- Shared (sub-pages) ---
	Back            UMETA(DisplayName = "Back")
};

// One rendered tile. The C++ state machine builds an ordered array of these per page (text + icon +
// gating) and hands it to the BP view via a BlueprintImplementableEvent. The view only draws; it
// never decides enable-state or layout order.
USTRUCT(BlueprintType)
struct FToolTile
{
	GENERATED_BODY()

	// The intent this tile emits on confirm.
	UPROPERTY(BlueprintReadOnly, Category = "Tools HUD")
	EHUDAction Action = EHUDAction::None;

	// Display label (localizable). Defaults come from UToolsHUDWidget::ActionLabels.
	UPROPERTY(BlueprintReadOnly, Category = "Tools HUD")
	FText Label;

	// Optional icon for the tile. Assigned per-action in the widget defaults (ActionIcons).
	UPROPERTY(BlueprintReadOnly, Category = "Tools HUD")
	TObjectPtr<UTexture2D> Icon = nullptr;

	// When false the tile is greyed out and cannot be confirmed (e.g. not enough regolith). Decided
	// by the gating delegate on the widget; the view just renders the disabled style.
	UPROPERTY(BlueprintReadOnly, Category = "Tools HUD")
	bool bEnabled = true;

	// Human-readable reason shown when a disabled tile is highlighted (tooltip / status line).
	UPROPERTY(BlueprintReadOnly, Category = "Tools HUD")
	FText DisabledReason;
};
