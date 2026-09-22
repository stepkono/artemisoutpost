// Fill out your copyright notice in the Description page of Project Settings.


#include "MoonResourceProviderData.h"

TSharedPtr<FJsonObject> UMoonResourceProviderData::BuildJsonFromData(const EGameEventType GameEventType)
{
	return TSharedPtr<FJsonObject>(new FJsonObject());
}