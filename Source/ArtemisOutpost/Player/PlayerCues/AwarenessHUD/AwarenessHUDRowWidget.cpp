// Fill out your copyright notice in the Description page of Project Settings.

#include "AwarenessHUDRowWidget.h"

#include "Components/Border.h"
#include "Components/TextBlock.h"

void UAwarenessHUDRowWidget::SetRow(const FAwarenessHUDRow& InRow)
{
	Row = InRow;

	if (NumberText)
	{
		NumberText->SetText(FText::AsNumber(Row.PlayerNumber));
	}
	if (ActivityText)
	{
		ActivityText->SetText(Row.ActivityText);
	}
	if (TagBorder)
	{
		TagBorder->SetBrushColor(ColorForPlayerNumber(Row.PlayerNumber));
	}
	if (TalkFrame)
	{
		TalkFrame->SetBrushColor(Row.bTalking ? TalkingColor : SilentColor);
	}

	OnRowSet(Row);
}

FLinearColor UAwarenessHUDRowWidget::ColorForPlayerNumber(int32 PlayerNumber) const
{
	if (PlayerColors.Num() == 0)
	{
		return FLinearColor::White;
	}
	const int32 Index = FMath::Max(PlayerNumber - 1, 0) % PlayerColors.Num();
	return PlayerColors[Index];
}
