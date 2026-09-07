// Fill out your copyright notice in the Description page of Project Settings.


#include "ProviderDataBase.h"

TSharedPtr<FJsonObject> UProviderDataBase::BuildJsonFromData(const EGameEventType GameEventType)
{
	// Subclasses provide the payload. Reaching this means a provider type has no override,
	// so log it rather than silently sending nothing.
	UE_LOG(LogTemp, Error, TEXT("UProviderDataBase: BuildJsonFromData is not overridden by %s. Returning null."),
		*GetClass()->GetName());

	return nullptr;
}
