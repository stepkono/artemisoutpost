// Fill out your copyright notice in the Description page of Project Settings.

#include "ARWorldCue.h"

#include "ArtemisOutpost/Moon/Transformation/VisualScaleSyncComponent.h"

AARWorldCue::AARWorldCue()
{
	CueContext = ERVMode::AR;

	VisualScaleSync = CreateDefaultSubobject<UVisualScaleSyncComponent>(TEXT("VisualScaleSync"));
}
