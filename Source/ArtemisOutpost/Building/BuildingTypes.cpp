// Fill out your copyright notice in the Description page of Project Settings.

#include "BuildingTypes.h"

bool UBuildingStatics::ToolActionToBuildingType(EToolAction Action, EOutpostBuildingType& OutType)
{
	switch (Action)
	{
	case EToolAction::BuildHabitat:    OutType = EOutpostBuildingType::Habitat;    return true;
	case EToolAction::BuildSolarPanel: OutType = EOutpostBuildingType::SolarPanel; return true;
	case EToolAction::BuildAntenna:    OutType = EOutpostBuildingType::Antenna;    return true;
	default:                           return false;
	}
}
