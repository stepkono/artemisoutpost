// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArtemisOutpost/Miscellaneous/DataTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "DataProviderSubsystemBase.generated.h"

/**
 * 
 */
UCLASS()
class ARTEMISOUTPOST_API UDataProviderSubsystemBase : public UWorldSubsystem
{
	GENERATED_BODY()
	
public: 
	EDataProviderType GetType() const; 
	
private: 
	EDataProviderType ProviderType; 
};
