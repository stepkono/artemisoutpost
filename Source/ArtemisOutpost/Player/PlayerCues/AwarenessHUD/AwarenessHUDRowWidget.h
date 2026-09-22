// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AwarenessHUDWidget.h"
#include "AwarenessHUDRowWidget.generated.h"

class UTextBlock;
class UBorder;

/**
 * C++ base for WBP_AwarenessRow: ONE player line in the HUD overview. UAwarenessHUDWidget creates
 * these (RowWidgetClass), pools them and calls SetRow about 5 Hz while the HUD is open.
 *
 * Zero-graph setup: name the widgets in the BP exactly as the BindWidgetOptional properties below and
 * the base fills them. Anything else (icons, animations) goes into OnRowSet.
 *
 *   TalkFrame     Border   outer frame, brush colour = TalkingColor while the player talks
 *   TagBorder     Border   colour tag, brush colour = ColorForPlayerNumber(PlayerNumber)
 *   NumberText    TextBlock the player number
 *   ActivityText  TextBlock the activity line
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class ARTEMISOUTPOST_API UAwarenessHUDRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetRow(const FAwarenessHUDRow& InRow);

	UFUNCTION(BlueprintPure, Category = "Awareness HUD")
	const FAwarenessHUDRow& GetRow() const { return Row; }

	// Palette lookup, wraps for numbers beyond the palette. Number 1 = PlayerColors[0].
	UFUNCTION(BlueprintPure, Category = "Awareness HUD")
	FLinearColor ColorForPlayerNumber(int32 PlayerNumber) const;

protected:
	// Called after the bound widgets were filled. Use it for anything the base does not cover.
	UFUNCTION(BlueprintImplementableEvent, Category = "Awareness HUD")
	void OnRowSet(const FAwarenessHUDRow& InRow);

	// ---- Optional bound widgets (match the names in the BP designer) ----
	UPROPERTY(BlueprintReadOnly, Category = "Awareness HUD", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> TalkFrame;

	UPROPERTY(BlueprintReadOnly, Category = "Awareness HUD", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> TagBorder;

	UPROPERTY(BlueprintReadOnly, Category = "Awareness HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NumberText;

	UPROPERTY(BlueprintReadOnly, Category = "Awareness HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ActivityText;

	// ---- Designer-editable styling ----
	// The colour tag per player number (1-based). Keep it consistent with every other cue that
	// colours by player number (beacons, avatar tags).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Awareness HUD|Style")
	TArray<FLinearColor> PlayerColors = {
		FLinearColor(0.95f, 0.35f, 0.20f), // 1 orange-red
		FLinearColor(0.20f, 0.55f, 0.95f), // 2 blue
		FLinearColor(0.30f, 0.85f, 0.35f), // 3 green
		FLinearColor(0.95f, 0.85f, 0.20f), // 4 yellow
	};

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Awareness HUD|Style")
	FLinearColor TalkingColor = FLinearColor(0.25f, 0.85f, 0.35f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Awareness HUD|Style")
	FLinearColor SilentColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);

private:
	UPROPERTY(Transient)
	FAwarenessHUDRow Row;
};
