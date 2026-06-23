// Fill out your copyright notice in the Description page of Project Settings.

#include "NetUtils.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

namespace
{
	bool TryGetNetMode(const UObject* WorldContextObject, ENetMode& OutNetMode)
	{
		if (!GEngine)
		{
			return false;
		}
		const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
		if (!World)
		{
			return false;
		}
		OutNetMode = World->GetNetMode();
		return true;
	}
}

bool UArtemisNetLibrary::IsServerHost(const UObject* WorldContextObject)
{
	ENetMode NetMode;
	if (TryGetNetMode(WorldContextObject, NetMode))
	{
		return ArtemisNet::IsServerHost(NetMode);
	}
	return false;
}

bool UArtemisNetLibrary::IsClientContext(const UObject* WorldContextObject)
{
	ENetMode NetMode;
	if (TryGetNetMode(WorldContextObject, NetMode))
	{
		return ArtemisNet::IsClientContext(NetMode);
	}
	return false;
}
