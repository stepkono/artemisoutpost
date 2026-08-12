// Fill out your copyright notice in the Description page of Project Settings.


#include "MiniGameConnectionUI.h"

void UMiniGameConnectionUI::SetOwner(AMinigameActor* Owner)
{
	MiniGameOwner = Owner;
}

void UMiniGameConnectionUI::SetButtonText(const FString& Text) const
{
	TextBlock->SetText(FText::FromString(Text));
}
