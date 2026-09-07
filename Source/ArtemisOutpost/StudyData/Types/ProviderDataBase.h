// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArtemisOutpost/Miscellaneous/DataTypes.h"
#include "UObject/Object.h"
#include "ProviderDataBase.generated.h"

/**
 *
 */
UCLASS()
class ARTEMISOUTPOST_API UProviderDataBase : public UObject
{
	GENERATED_BODY()
	
public:
	UPROPERTY()
	TEnumAsByte<EGameEventType> GameEventType; 
	
	virtual TSharedPtr<FJsonObject> BuildJsonFromData(EGameEventType GameEventType);
};
