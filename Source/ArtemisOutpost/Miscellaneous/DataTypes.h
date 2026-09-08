// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DataTypes.generated.h"

class ACharVR;
class AMasterRover;
class APawnController;

USTRUCT()
struct FOrderedAnchorsPositions
{
	GENERATED_BODY()

	UPROPERTY()
	FVector AAnchorPos;

	UPROPERTY()
	FVector BAnchorPos;

	UPROPERTY()
	FVector CAnchorPos;

	UPROPERTY()
	FVector DAnchorPos;
};

USTRUCT()
struct FAnchorsActors
{
	GENERATED_BODY()

	UPROPERTY()
	AActor* AAnchor;

	UPROPERTY()
	AActor* BAnchor;

	UPROPERTY()
	AActor* CAnchor;

	UPROPERTY()
	AActor* DAnchor;
};

USTRUCT(BlueprintType)
struct FCoordinates
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	float Latitude = 0;

	UPROPERTY(BlueprintReadWrite)
	float Longitude = 0;

	UPROPERTY(BlueprintReadWrite)
	float Height = 0;
};

USTRUCT(BlueprintType)
struct FMapBaseCoordinates
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	FCoordinates UpLeft;

	UPROPERTY(BlueprintReadWrite)
	FCoordinates BottomLeft;

	UPROPERTY(BlueprintReadWrite)
	FCoordinates BottomRight;

	UPROPERTY(BlueprintReadWrite)
	FCoordinates Origin;

	UPROPERTY(BlueprintReadWrite)
	float TerrainElevation;
};

USTRUCT(BlueprintType)
struct FControlCommand
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	float Accelerator = 0.0f;

	UPROPERTY(BlueprintReadWrite)
	float Steering = 0.0f;
};

USTRUCT()
struct FCalibratedData
{
	GENERATED_BODY()

	UPROPERTY()
	FVector AAnchorPos;

	UPROPERTY()
	FVector BAnchorPos;

	UPROPERTY()
	FVector CAnchorPos;

	UPROPERTY()
	FVector DAnchorPos;

	UPROPERTY()
	FVector PlaneCenter;

	UPROPERTY()
	FVector PlaneNormal;
};

USTRUCT(BlueprintType)
struct FArtemisPlayer
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly)
	APawnController* PawnController = nullptr;

	UPROPERTY(BlueprintReadOnly)
	FString UPID;

	UPROPERTY(BlueprintReadOnly)
	int PlayerNumber = -1;

	UPROPERTY(BlueprintReadOnly)
	AMasterRover* MasterRover = nullptr;

	UPROPERTY(BlueprintReadOnly)
	ACharVR* VRChar = nullptr;
};

UENUM(BlueprintType)
enum EBuildingType
{
	NoneBuilding UMETA(DisplayName = "None"),
	SOLAR_PANEL UMETA(DisplayName = "Solar Panel"),
	HABITAT UMETA(DisplayName = "Habitat"),
};

UENUM(BlueprintType)
enum ERessourceType
{
	NoneRessource UMETA(DisplayName = "None"), 
	REGOLITH UMETA(DisplayName = "Regular"),
};

USTRUCT(BlueprintType)
struct FAreaScan
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadWrite)
	FVector GeoPosition; 
	
	UPROPERTY(BlueprintReadWrite)
	FArtemisPlayer Authorship; 
};

USTRUCT(BlueprintType)
struct FBuilding
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadWrite)
	TEnumAsByte<EBuildingType> BuildingType;
};

USTRUCT(BlueprintType)
struct FPositionMetaData
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadWrite)
	bool bIsExplored = false; 
	
	UPROPERTY(BlueprintReadWrite)
	TEnumAsByte<ERessourceType> RessourceType = ERessourceType::NoneRessource;
	
	UPROPERTY(BlueprintReadWrite)
	TEnumAsByte<EBuildingType> BuildingType = EBuildingType::NoneBuilding;	
};

UENUM(BlueprintType)
enum EXRMode
{
	VR UMETA(DisplayName = "VR"),
	AR UMETA(DisplayName = "AR"),
};

UENUM(BlueprintType)
enum EDataProviderType
{
	MoonResource UMETA(DisplayName = "Moon Resource"),
	MoonBuilding UMETA(DisplayName = "Moon Building"), 
	MoonScanner  UMETA(DisplayName = "Moon Scanner"),
	PlayerAction UMETA(DisplayName = "Player Action"),
};

UENUM(BlueprintType)
enum EGameEventType
{
	NewMiniGamePlaced    UMETA(DisplayName = "New Minigame Placed"),
	
	ResourceDiscovered    UMETA(DisplayName = "Resource Discovered"),
	ResourceMined   UMETA(DisplayName = "Resource Mined"),
	
	PlayerMovementStart  UMETA(DisplayName = "Player Movement Start"),
	PlayerMovementFinish UMETA(DisplayName = "Player Movement Finish"),
	PlayerLookAtStart    UMETA(DisplayName = "Player Looking At Start"),
	PlayerLookAtFinish   UMETA(DisplayName = "Player Looking At Finish"),
	
	PlayerConnected      UMETA(DisplayName = "Player Connected"),
	PlayerDisconnected   UMETA(DisplayName = "Player Disconnected"),

	// HMD worn-state, distinct from network connect/disconnect: a doffed HMD does not necessarily
	// mean the client has dropped its connection.
	HMDDonned            UMETA(DisplayName = "HMD Donned"),
	HMDDoffed            UMETA(DisplayName = "HMD Doffed"),
};

/**
 * 
 */
class ARTEMISOUTPOST_API DataTypes
{
public:
	DataTypes();
	~DataTypes();
};
