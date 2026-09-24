// Fill out your copyright notice in the Description page of Project Settings.

#include "AnchorsCueVisualizer.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "ArtemisOutpost/Anchors/AnchorsManagerSubsystem.h"

AAnchorsCueVisualizer::AAnchorsCueVisualizer()
{
	AnchorA = CreateDefaultSubobject<USceneComponent>(TEXT("AnchorA"));
	AnchorA->SetupAttachment(Root);

	AnchorB = CreateDefaultSubobject<USceneComponent>(TEXT("AnchorB"));
	AnchorB->SetupAttachment(Root);

	AnchorC = CreateDefaultSubobject<USceneComponent>(TEXT("AnchorC"));
	AnchorC->SetupAttachment(Root);
	
	AnchorD = CreateDefaultSubobject<USceneComponent>(TEXT("AnchorD"));
	AnchorD->SetupAttachment(Root);
}

const USceneComponent* AAnchorsCueVisualizer::GetAnchorComponent(int32 Index) const
{
	switch (Index)
	{
	case 0: return AnchorA;
	case 1: return AnchorB;
	case 2: return AnchorC;
	case 3: return AnchorD;
	default: return nullptr;
	}
}

bool AAnchorsCueVisualizer::RefreshAnchorsFrame()
{
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const UAnchorsManagerSubsystem* AnchorsManager = GameInstance ? GameInstance->GetSubsystem<UAnchorsManagerSubsystem>() : nullptr;

	FTransform Frame;
	const bool bNowHasFrame = AnchorsManager && AnchorsManager->GetAnchorsFrameTransform(Frame);
	if (bNowHasFrame != bHasAnchorsFrame)
	{
		UE_LOG(LogTemp, Log, TEXT("[VRCues] %s anchors frame %s"), *GetName(), bNowHasFrame ? TEXT("ready") : TEXT("lost"));
	}

	bHasAnchorsFrame = bNowHasFrame;
	if (bHasAnchorsFrame)
	{
		AnchorsFrame = Frame;
	}
	return bHasAnchorsFrame;
}
