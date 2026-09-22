// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerActionProvider.h"

#include "ArtemisOutpost/StudyData/Types/PlayerActionType/PlayerActionProviderData.h"

void UPlayerActionProvider::ServerReportHmdState(const FString& InUPID, bool bWorn)
{
	UPlayerActionProviderData* Data = NewObject<UPlayerActionProviderData>(this);
	Data->UPID  = InUPID;
	Data->bWorn = bWorn;

	const EGameEventType Event = bWorn ? EGameEventType::HMDDonned : EGameEventType::HMDDoffed;

	UE_LOG(LogTemp, Warning, TEXT("[HMD] Provider broadcasting %s (bound=%d)"),
		*UEnum::GetValueAsString(Event), OnPlayerActionEvent.IsBound() ? 1 : 0);

	OnPlayerActionEvent.Broadcast(*Data, Event);
}

void UPlayerActionProvider::ServerReportCueEvent(const FString& InUPID, const FPlayerCueState& State, EGameEventType Event)
{
	UPlayerActionProviderData* Data = NewObject<UPlayerActionProviderData>(this);
	Data->UPID     = InUPID;
	Data->bWorn    = State.bHmdWorn;
	Data->CueState = State;

	UE_LOG(LogTemp, Log, TEXT("[Cues] Provider broadcasting %s for '%s' (bound=%d)"),
		*UEnum::GetValueAsString(Event), *InUPID, OnPlayerActionEvent.IsBound() ? 1 : 0);

	OnPlayerActionEvent.Broadcast(*Data, Event);
}
