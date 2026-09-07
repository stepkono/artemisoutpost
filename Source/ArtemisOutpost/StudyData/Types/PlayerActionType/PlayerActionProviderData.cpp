// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerActionProviderData.h"

TSharedPtr<FJsonObject> UPlayerActionProviderData::BuildJsonFromData(const EGameEventType GameEventType)
{
	switch (GameEventType)
	{
		case HMDDonned:
		case HMDDoffed:
			return SerializeHmdState(GameEventType);
		default:
			{
				UE_LOG(LogTemp, Error, TEXT("PlayerActionProviderData: BuildJsonFromData received an unhandled GameEventType. Returning null."));
				return nullptr;
			}
	}
}

TSharedPtr<FJsonObject> UPlayerActionProviderData::SerializeHmdState(const EGameEventType GameEventType)
{
	const TSharedPtr<FJsonObject> DataObject = MakeShared<FJsonObject>();
	DataObject->SetStringField(TEXT("UPID: "),  UPID);
	DataObject->SetStringField(TEXT("Event: "), UEnum::GetValueAsString(GameEventType));
	DataObject->SetBoolField(TEXT("HMDWorn: "),  bWorn);

	return DataObject;
}
