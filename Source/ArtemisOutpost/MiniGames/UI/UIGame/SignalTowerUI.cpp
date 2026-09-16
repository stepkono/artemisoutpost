// Fill out your copyright notice in the Description page of Project Settings.

#include "SignalTowerUI.h"

#define LOCTEXT_NAMESPACE "SignalTowerUI"

USignalTowerUI::USignalTowerUI(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Same German defaults the tower View always had. The WBP may override them in its defaults.
	AxisTakenReason  = LOCTEXT("AxisTaken",  "Von einem anderen Spieler belegt");
	AxisSolvedReason = LOCTEXT("AxisSolved", "Bereits ausgerichtet");
}

#undef LOCTEXT_NAMESPACE
