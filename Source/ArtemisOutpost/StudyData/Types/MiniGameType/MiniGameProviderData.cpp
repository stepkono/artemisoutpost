// Fill out your copyright notice in the Description page of Project Settings.


#include "MiniGameProviderData.h"

TSharedPtr<FJsonObject> UMiniGameProviderData::BuildJsonFromData(EGameEventType GameEventType)
{
	switch (GameEventType)
	{
		case EGameEventType::NewMiniGamePlaced: return SerializeNewMinigameData(); 
		default:
			{
				UE_LOG(LogTemp, Error, TEXT("MiniGameProviderData: BuildJsonFromData received an Unknown GameEventType. Retuning null."));
				return nullptr;
			}
	}
}

TSharedPtr<FJsonObject> UMiniGameProviderData::SerializeNewMinigameData()
{
	const TSharedPtr<FJsonObject> DataObject = MakeShared<FJsonObject>();
	DataObject->SetStringField(TEXT("MGID: "), MGID.ToString());
	DataObject->SetStringField(TEXT("Building Type: "), Type);
	
	const TSharedRef<FJsonObject> BuildingLocationObject = MakeShared<FJsonObject>();
	BuildingLocationObject->SetNumberField(TEXT("Lon: "),    BuildLocation.X);
	BuildingLocationObject->SetNumberField(TEXT("Lat: "),    BuildLocation.Y);
	BuildingLocationObject->SetNumberField(TEXT("Height: "), BuildLocation.Z);	
	DataObject->SetObjectField(TEXT("Build Location: "), BuildingLocationObject);
	
	return DataObject;
}