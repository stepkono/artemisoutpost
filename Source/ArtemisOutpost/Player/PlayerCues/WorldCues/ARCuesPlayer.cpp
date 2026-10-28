// Fill out your copyright notice in the Description page of Project Settings.

#include "ARCuesPlayer.h"

#include "Components/SceneComponent.h"

AARCuesPlayer::AARCuesPlayer()
{
	Head = CreateDefaultSubobject<USceneComponent>(TEXT("Head"));
	Head->SetupAttachment(Root);

	LeftHand = CreateDefaultSubobject<USceneComponent>(TEXT("LeftHand"));
	LeftHand->SetupAttachment(Root);

	RightHand = CreateDefaultSubobject<USceneComponent>(TEXT("RightHand"));
	RightHand->SetupAttachment(Root);
}
