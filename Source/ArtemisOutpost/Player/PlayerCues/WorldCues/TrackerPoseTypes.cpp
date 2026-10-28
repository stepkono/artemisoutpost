// Fill out your copyright notice in the Description page of Project Settings.

#include "TrackerPoseTypes.h"

#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"

double FTrackerPose::GetAgeSeconds(const UWorld* World) const
{
	if (!HasBeenWritten())
	{
		return NeverWrittenAgeSeconds;
	}
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!GameState)
	{
		return NeverWrittenAgeSeconds;
	}
	return FMath::Max(0.0, GameState->GetServerWorldTimeSeconds() - ServerWriteTime);
}
