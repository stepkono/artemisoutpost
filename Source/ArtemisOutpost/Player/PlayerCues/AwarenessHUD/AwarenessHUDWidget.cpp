// Fill out your copyright notice in the Description page of Project Settings.

#include "AwarenessHUDWidget.h"

#include "AwarenessHUDRowWidget.h"
#include "Components/PanelWidget.h"

#define LOCTEXT_NAMESPACE "AwarenessHUD"

void UAwarenessHUDWidget::SetRows(const TArray<FAwarenessHUDRow>& InRows, EPlayerContext InLocalContext)
{
	Rows = InRows;
	LocalContext = InLocalContext;

	FillBlock(EPlayerContext::AR, RowsAR, BlockRootAR, PoolAR);
	FillBlock(EPlayerContext::VR, RowsVR, BlockRootVR, PoolVR);
	FillBlock(EPlayerContext::R,  RowsR,  BlockRootR,  PoolR);
	ReorderBlocks();

	OnRowsUpdated(Rows, LocalContext);
}

void UAwarenessHUDWidget::NotifyOpened()
{
	OnOpened();
}

void UAwarenessHUDWidget::NotifyClosed()
{
	OnClosed();
}

// ---- Automatic blocks ----

void UAwarenessHUDWidget::FillBlock(EPlayerContext Context, UPanelWidget* Panel, UWidget* BlockRoot, TArray<TObjectPtr<UAwarenessHUDRowWidget>>& Pool)
{
	if (!Panel)
	{
		return; // this block is not authored in the BP
	}

	const TArray<FAwarenessHUDRow> BlockRows = GetRowsForContext(Context);

	if (BlockRoot && bCollapseEmptyBlocks)
	{
		BlockRoot->SetVisibility(BlockRows.Num() > 0 ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (!RowWidgetClass)
	{
		if (BlockRows.Num() > 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("[AwarenessHUD] %s: RowWidgetClass is not set, rows cannot be created."), *GetName());
		}
		return;
	}

	// Grow the pool to the needed size; row widgets are created once and reused.
	while (Pool.Num() < BlockRows.Num())
	{
		UAwarenessHUDRowWidget* RowWidget = CreateWidget<UAwarenessHUDRowWidget>(this, RowWidgetClass);
		if (!RowWidget)
		{
			return;
		}
		Panel->AddChild(RowWidget);
		Pool.Add(RowWidget);
	}

	for (int32 i = 0; i < Pool.Num(); ++i)
	{
		UAwarenessHUDRowWidget* RowWidget = Pool[i];
		if (!RowWidget)
		{
			continue;
		}
		if (i < BlockRows.Num())
		{
			RowWidget->SetRow(BlockRows[i]);
			RowWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		else
		{
			RowWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UAwarenessHUDWidget::ReorderBlocks()
{
	if (!BlocksBox)
	{
		return;
	}

	auto RootFor = [this](EPlayerContext C) -> UWidget*
	{
		switch (C)
		{
		case EPlayerContext::AR: return BlockRootAR;
		case EPlayerContext::VR: return BlockRootVR;
		default:                 return BlockRootR;
		}
	};

	int32 Index = 0;
	for (const EPlayerContext C : GetBlockOrder())
	{
		UWidget* Root = RootFor(C);
		if (Root && BlocksBox->HasChild(Root))
		{
			BlocksBox->ShiftChild(Index, Root);
			++Index;
		}
	}
}

// ---- Reads ----

TArray<FAwarenessHUDRow> UAwarenessHUDWidget::GetRowsForContext(EPlayerContext Context) const
{
	TArray<FAwarenessHUDRow> Result;
	for (const FAwarenessHUDRow& Row : Rows)
	{
		if (Row.Context == Context)
		{
			Result.Add(Row);
		}
	}
	Result.Sort([](const FAwarenessHUDRow& A, const FAwarenessHUDRow& B) { return A.PlayerNumber < B.PlayerNumber; });
	return Result;
}

TArray<EPlayerContext> UAwarenessHUDWidget::GetBlockOrder() const
{
	TArray<EPlayerContext> Order;
	Order.Add(LocalContext);
	for (const EPlayerContext C : { EPlayerContext::AR, EPlayerContext::VR, EPlayerContext::R })
	{
		if (C != LocalContext)
		{
			Order.Add(C);
		}
	}
	return Order;
}

// ---- Texts ----

FText UAwarenessHUDWidget::MakeTargetText(const FPointingTarget& Target)
{
	switch (Target.Kind)
	{
	case EPointingTargetKind::Surface:
		return LOCTEXT("TargetSurface", "Oberfläche");
	case EPointingTargetKind::Minigame:
		return UEnum::GetDisplayValueAsText(Target.MinigameType);
	case EPointingTargetKind::Player:
		return FText::Format(LOCTEXT("TargetPlayer", "Spieler {0}"), FText::AsNumber(Target.TargetPlayerNumber));
	case EPointingTargetKind::Rover:
		return FText::Format(LOCTEXT("TargetRover", "Rover {0}"), FText::AsNumber(Target.TargetPlayerNumber));
	default:
		return FText::GetEmpty();
	}
}

FText UAwarenessHUDWidget::MakeActivityText(const FPlayerCueState& State)
{
	switch (State.GetActivity())
	{
	case EPlayerActivity::InMinigame:
		return FText::Format(LOCTEXT("ActivityMinigame", "Minigame: {0}"), UEnum::GetDisplayValueAsText(State.MinigameType));
	case EPlayerActivity::Pointing:
		return FText::Format(LOCTEXT("ActivityPointing", "Zeigt auf {0}"), MakeTargetText(State.PointerTarget));
	case EPlayerActivity::UsingTool:
		return UEnum::GetDisplayValueAsText(State.ToolActivity);
	case EPlayerActivity::Walking:
		return LOCTEXT("ActivityWalking", "Geht");
	case EPlayerActivity::Idle:
	default:
		return LOCTEXT("ActivityIdle", "Idle");
	}
}

#undef LOCTEXT_NAMESPACE
