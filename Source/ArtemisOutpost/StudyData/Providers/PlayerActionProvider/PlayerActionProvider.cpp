// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerActionProvider.h"

#include "ArtemisOutpost/StudyData/Types/PlayerActionType/PlayerActionProviderData.h"

void UPlayerActionProvider::ServerReportHmdState(const FString& InUPID, bool bWorn)
{
	UPlayerActionProviderData* Data = NewObject<UPlayerActionProviderData>(this);
	Data->UPID  = InUPID;
	Data->bWorn = bWorn;

	const EGameEventType Event = bWorn ? EGameEventType::HMDDonned : EGameEventType::HMDDoffed;
	OnPlayerActionEvent.Broadcast(*Data, Event);
}
