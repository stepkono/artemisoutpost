// Fill out your copyright notice in the Description page of Project Settings.


#include "DataProviderSubsystemBase.h"
#include "ArtemisOutpost/Miscellaneous/DataTypes.h"

EDataProviderType UDataProviderSubsystemBase::GetType() const
{
	return ProviderType; 
}
