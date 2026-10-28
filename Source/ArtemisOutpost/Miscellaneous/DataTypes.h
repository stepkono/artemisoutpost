// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DataTypes.generated.h"

class ACharVR;
class AMasterRover;
class APawnController;
class AArtemisPlayerState;

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

	bool operator==(const FCoordinates& Other) const
	{
		return Latitude == Other.Latitude && Longitude == Other.Longitude && Height == Other.Height;
	}
	bool operator!=(const FCoordinates& Other) const { return !(*this == Other); }
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
	float TerrainElevation = 0;

	bool operator==(const FMapBaseCoordinates& Other) const
	{
		return UpLeft == Other.UpLeft && BottomLeft == Other.BottomLeft && BottomRight == Other.BottomRight
			&& Origin == Other.Origin && TerrainElevation == Other.TerrainElevation;
	}
	bool operator!=(const FMapBaseCoordinates& Other) const { return !(*this == Other); }

	// False while nothing was received yet (all zero). A real map region never has identical corners.
	bool IsSet() const { return *this != FMapBaseCoordinates(); }
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
	
	// Server-only: a PlayerController never exists on other clients, so it is kept out of the roster replication.
	UPROPERTY(BlueprintReadOnly, NotReplicated)
	APawnController* PawnController = nullptr;

	// The player's current PlayerState. Null while the player is disconnected, a reconnect gets a fresh one.
	UPROPERTY(BlueprintReadOnly)
	AArtemisPlayerState* PlayerState = nullptr;

	UPROPERTY(BlueprintReadOnly)
	FString UPID;

	UPROPERTY(BlueprintReadOnly)
	int PlayerNumber = -1;

	UPROPERTY(BlueprintReadOnly)
	AMasterRover* MasterRover = nullptr;

	UPROPERTY(BlueprintReadOnly)
	ACharVR* VRChar = nullptr;
	
	UPROPERTY(BlueprintReadOnly)
	FLinearColor PlayerColor; 
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

// The context a player is currently in. "R" = Reality: the HMD is doffed, we cannot know whether the
// person sits at the PC or just took the headset off, so we do not claim more than "not in XR".
UENUM(BlueprintType)
enum class ERVMode : uint8
{
	VR UMETA(DisplayName = "VR"),
	AR UMETA(DisplayName = "AR"),
	R UMETA(DisplayName = "R"),
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

	// Awareness cues (see AArtemisPlayerState). Event stream, produced server-side on every change of
	// the replicated cue state; PointingUpdate is the only rate-limited one.
	PlayerContextChanged  UMETA(DisplayName = "Player Context Changed"),
	PlayerActivityChanged UMETA(DisplayName = "Player Activity Changed"),
	PlayerPointingStart   UMETA(DisplayName = "Player Pointing Start"),
	PlayerPointingUpdate  UMETA(DisplayName = "Player Pointing Update"),
	PlayerPointingFinish  UMETA(DisplayName = "Player Pointing Finish"),
	PlayerTalkStart       UMETA(DisplayName = "Player Talk Start"),
	PlayerTalkFinish      UMETA(DisplayName = "Player Talk Finish"),
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
