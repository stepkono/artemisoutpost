// Fill out your copyright notice in the Description page of Project Settings.

#include "MiniGameUI.h"

void UMiniGameUI::EmitInput(FMinigameInput Input)
{
	OnInput.Broadcast(Input);
}
