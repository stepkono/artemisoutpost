// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArtemisOutpost/Miscellaneous/DataTypes.h"
#include "UObject/Object.h"
#include "ProviderDataBase.generated.h"

/**
 *
 */
UCLASS(Abstract)
class ARTEMISOUTPOST_API UProviderDataBase : public UObject
{
	GENERATED_BODY()

public:
	// Override per provider type. The base logs and returns null, see ProviderDataBase.cpp.
	virtual TSharedPtr<FJsonObject> BuildJsonFromData(const EGameEventType GameEventType);
};
