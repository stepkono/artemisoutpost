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
