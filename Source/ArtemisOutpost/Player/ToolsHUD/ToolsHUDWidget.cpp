// Fill out your copyright notice in the Description page of Project Settings.

#include "ToolsHUDWidget.h"

#define LOCTEXT_NAMESPACE "ToolsHUD"

// ---- Page layout (single source of truth for tile order) ----

TArray<EHUDAction> UToolsHUDWidget::GetActionsForPage(EToolHUDPage Page)
{
	switch (Page)
	{
	case EToolHUDPage::Building:
		return { EHUDAction::BuildHabitat, EHUDAction::BuildSolarPanel, EHUDAction::BuildAntenna, EHUDAction::Back };
	case EToolHUDPage::Scanning:
		return { EHUDAction::ScanEnvironment, EHUDAction::ScanResource, EHUDAction::Back };
	case EToolHUDPage::Main:
	default:
		return { EHUDAction::OpenBuilding, EHUDAction::OpenScanning, EHUDAction::CloseMenu };
	}
}

FText UToolsHUDWidget::DefaultLabelFor(EHUDAction Action)
{
	switch (Action)
	{
	case EHUDAction::OpenBuilding:    return LOCTEXT("BuildingMode",  "Building Mode");
	case EHUDAction::OpenScanning:    return LOCTEXT("ScanningMode",  "Scanning Mode");
	case EHUDAction::CloseMenu:       return LOCTEXT("CloseMenu",     "Schließen");
	case EHUDAction::BuildHabitat:    return LOCTEXT("Habitat",       "Habitat");
	case EHUDAction::BuildSolarPanel: return LOCTEXT("SolarPanel",    "Solar Panel");
	case EHUDAction::BuildAntenna:    return LOCTEXT("Antenna",       "Funkmast");
	case EHUDAction::ScanEnvironment: return LOCTEXT("Environment",   "Umgebung");
	case EHUDAction::ScanResource:    return LOCTEXT("Resource",      "Ressource");
	case EHUDAction::Back:            return LOCTEXT("Back",          "Zurück");
	default:                           return FText::GetEmpty();
	}
}

// ---- Lifecycle / page switching ----

void UToolsHUDWidget::OpenToPage(EToolHUDPage Page)
{
	OnMenuOpened();
	CurrentPage = Page;
	RebuildTiles();
}

void UToolsHUDWidget::GoToPage(EToolHUDPage Page)
{
	CurrentPage = Page;
	RebuildTiles();
}

void UToolsHUDWidget::NotifyClosed()
{
	OnMenuClosed();
}

void UToolsHUDWidget::RebuildTiles()
{
	CurrentTiles.Reset();

	for (const EHUDAction Action : GetActionsForPage(CurrentPage))
	{
		FToolTile Tile;
		Tile.Action = Action;

		if (const FText* Override = ActionLabels.Find(Action))
		{
			Tile.Label = *Override;
		}
		else
		{
			Tile.Label = DefaultLabelFor(Action);
		}

		Tile.Icon = ActionIcons.FindRef(Action);
		Tile.bEnabled = EvaluateEnabled(Action, Tile.DisabledReason);

		CurrentTiles.Add(Tile);
	}

	// Reset navigation state for the new page and let the view rebuild, then place the highlight on
	// the first tile (OldIndex = INDEX_NONE signals "no previous highlight").
	HighlightIndex = 0;

	OnPageBuilt(CurrentPage, CurrentTiles);

	if (CurrentTiles.Num() > 0)
	{
		OnHighlightChanged(HighlightIndex, INDEX_NONE);
	}
}

void UToolsHUDWidget::RefreshGating()
{
	// Re-evaluate enabled-state in place (resources may have changed while the menu is open) and let
	// the view re-render. Highlight index is preserved.
	for (FToolTile& Tile : CurrentTiles)
	{
		Tile.bEnabled = EvaluateEnabled(Tile.Action, Tile.DisabledReason);
	}

	OnPageBuilt(CurrentPage, CurrentTiles);

	if (CurrentTiles.IsValidIndex(HighlightIndex))
	{
		OnHighlightChanged(HighlightIndex, HighlightIndex);
	}
}

// ---- Navigation ----

void UToolsHUDWidget::HandleNavigate(FVector2D Axis)
{
	const float Mag = Axis.Size();

	// Stick returned near centre: re-arm so the very next push steps immediately (snappy flicks).
	if (Mag < NavReleaseThreshold)
	{
		LastNavStepTime = -1000.0f;
		return;
	}

	// Not a firm-enough push yet.
	if (Mag < NavStepThreshold)
	{
		return;
	}

	// Firm push: step on a fresh push, then auto-repeat every NavRepeatDelay while held. Time-based
	// (not "wait for centre") so it stays responsive even though Enhanced Input stops calling us once
	// the stick drops below its actuation threshold.
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : LastNavStepTime + NavRepeatDelay;
	if ((Now - LastNavStepTime) < NavRepeatDelay)
	{
		return;
	}

	// Dominant-axis stepping, tiles read in order: right / down = next, left / up = previous.
	// (EnhancedInput thumbstick Y is +1 up, so a negative Y means "down".)
	const int32 Dir = (FMath::Abs(Axis.X) >= FMath::Abs(Axis.Y))
		? (Axis.X > 0.f ? 1 : -1)
		: (Axis.Y < 0.f ? 1 : -1);

	SetHighlight(HighlightIndex + Dir);
	LastNavStepTime = Now;
}

void UToolsHUDWidget::SetHighlight(int32 NewIndex)
{
	const int32 Num = CurrentTiles.Num();
	if (Num == 0)
	{
		return;
	}

	if (bWrapNavigation)
	{
		NewIndex = ((NewIndex % Num) + Num) % Num;
	}
	else
	{
		NewIndex = FMath::Clamp(NewIndex, 0, Num - 1);
	}

	if (NewIndex == HighlightIndex)
	{
		return;
	}

	const int32 OldIndex = HighlightIndex;
	HighlightIndex = NewIndex;
	OnHighlightChanged(HighlightIndex, OldIndex);
}

// ---- Confirm ----

void UToolsHUDWidget::HandleConfirm()
{
	if (!CurrentTiles.IsValidIndex(HighlightIndex))
	{
		return;
	}

	const FToolTile& Tile = CurrentTiles[HighlightIndex];

	// Always give the view press feedback; bWasEnabled lets it choose press vs. "denied" bump.
	OnTileConfirmed(HighlightIndex, Tile.bEnabled);

	if (Tile.bEnabled)
	{
		OnToolActionConfirmed.Broadcast(Tile.Action);
	}
}

// ---- Gating ----

bool UToolsHUDWidget::EvaluateEnabled(EHUDAction Action, FText& OutReason) const
{
	OutReason = FText::GetEmpty();

	if (AvailabilityDelegate.IsBound())
	{
		return AvailabilityDelegate.Execute(Action, OutReason);
	}

	// No provider bound -> everything is available (design-time / early bring-up).
	return true;
}

#undef LOCTEXT_NAMESPACE
