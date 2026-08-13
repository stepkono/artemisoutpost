// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ArtemisOutpost/Player/ToolsHUD/ToolsHUDTypes.h"
#include "BuildingTypes.generated.h"

// A placeable outpost building. Each maps to a concrete AMinigameActor subclass via the
// BuildingClasses map on APawnController (Antenna -> ASignalTower today). Extend by adding a value
// here, a case in UBuildingStatics::ToolActionToBuildingType, and a row in the map.
UENUM(BlueprintType)
enum class EOutpostBuildingType : uint8
{
	Habitat    UMETA(DisplayName = "Habitat"),
	SolarPanel UMETA(DisplayName = "Solar Panel"),
	Antenna    UMETA(DisplayName = "Antenna / Funkmast")
};

// Small BP-callable bridge so the placement Blueprint can turn the HUD's EToolAction into a building
// type without a hand-maintained switch in Blueprint.
UCLASS()
class ARTEMISOUTPOST_API UBuildingStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Returns true and fills OutType when Action is one of the Build* actions; false otherwise
	// (Scan*/navigation/close), leaving OutType untouched.
	UFUNCTION(BlueprintPure, Category = "Building")
	static bool ToolActionToBuildingType(EToolAction Action, EOutpostBuildingType& OutType);
};
