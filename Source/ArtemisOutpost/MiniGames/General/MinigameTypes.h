// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"
#include "MinigameTypes.generated.h"

// A placeable outpost building. Each maps to a concrete AMinigameActor subclass via the
// BuildingClasses map on ABuildingTool (Antenna -> ASignalTower today). Named with the Outpost
// prefix because plain "EBuildingType" collides with an engine/plugin enum.
UENUM(BlueprintType)
enum class EMiniGameType : uint8
{
	SignalTower  UMETA(DisplayName = "SignalTower"),
	Habitat      UMETA(DisplayName = "Habitat"),
	SolarPanel   UMETA(DisplayName = "Solar Panel"),
};

// What the scanning tool scans. Surface = the ground at the aim point; Area = the environment around
// the player (a radius, no aiming). Both are hold-to-scan.
UENUM(BlueprintType)
enum class EScanMode : uint8
{
	Surface UMETA(DisplayName = "Surface Scan"),
	Mining  UMETA(DisplayName = "Ressource Mining"),
	Area    UMETA(DisplayName = "Area Scan")
};

// Lifecycle state of a minigame instance. Server-authoritative, replicated to all views.
UENUM(BlueprintType)
enum class EMinigameState : uint8
{
	Idle       UMETA(DisplayName = "Idle"),
	Active     UMETA(DisplayName = "Active"),
	Committing UMETA(DisplayName = "Committing"),
	Completed  UMETA(DisplayName = "Completed"),
	Failed     UMETA(DisplayName = "Failed")
};

UENUM(BlueprintType)
enum class EInputActionType : uint8
{
	Triggered    UMETA(DisplayName = "Triggered"),
	Started      UMETA(DisplayName = "Started"),
	Ongoing      UMETA(DisplayName = "Ongoing"),
	Canceled     UMETA(DisplayName = "Canceled"),
	Completed    UMETA(DisplayName = "Completed"),
};

// One occupied VR manipulation slot. Array position is the slot index. Kept as a struct (not a
// bare UPID) because per-participant logging/coupling metadata will live here later (§11).
USTRUCT(BlueprintType)
struct FConnectionSlot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Connection")
	FString OwnerUPID;
};

// One controllable degree of freedom of a coupled task (Signal Tower: [0]=Earth, [1]=Habitat).
// The server mutates Value from input; clients render beams / UI from the replicated copy.
USTRUCT(BlueprintType)
struct FAxisData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Minigame")
	int AxisIndex = -1;
	
	// Current angle in degrees (0..360).
	UPROPERTY(BlueprintReadOnly, Category = "Minigame")
	float Value = 0.0f;

	// Seconds this axis has continuously been within tolerance (server-driven dwell).
	UPROPERTY(BlueprintReadOnly, Category = "Minigame")
	float InToleranceTime = 0.0f;

	// UPID that exclusively controls this axis. Empty = any participant may (the solo case).
	UPROPERTY(BlueprintReadOnly, Category = "Minigame")
	FString OwnerUPID;
};

// One-time target packet for the puppet (sent via the generic ApplyData channel, separate from the
// per-rotation FAxisData). The target is a fixed per-game property, transmitted once — not part of
// the live axis stream.
USTRUCT(BlueprintType)
struct FAxisTargetData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Minigame")
	int32 AxisIndex = -1;

	UPROPERTY(BlueprintReadOnly, Category = "Minigame")
	float TargetDeg = 0.0f;
};

// What an input intent does. Claim/Release manage which participant owns an axis (the
// axis-selection screen); Rotate turns the owned axis.
UENUM(BlueprintType)
enum class EMinigameInputType : uint8
{
	ClaimAxis   UMETA(DisplayName = "Claim Axis"),
	ReleaseAxis UMETA(DisplayName = "Release Axis"),
	Rotate      UMETA(DisplayName = "Rotate")
};

// An input intent from a participant, sent client -> server. For Rotate, Delta is a signed
// angular increment in DEGREES (never an absolute angle), so lost/reordered messages only lose
// a small step and stay robust under latency (§9). The server clamps it per message.
USTRUCT(BlueprintType)
struct FMinigameInput
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Minigame")
	EMinigameInputType Type = EMinigameInputType::Rotate;

	UPROPERTY(BlueprintReadWrite, Category = "Minigame")
	int32 AxisIndex = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Minigame")
	float Delta = 0.0f;
};

// Server-side registry record for ONE placed minigame/building, held by UMoonMiniGamesManager keyed
// on MGID. Common fields for every type; the type-specific payload rides in MiniGameData (e.g. a
// FHabitatData for habitats), so a single map covers all minigames.
USTRUCT(BlueprintType)
struct FMiniGameRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Buildings")
	FGuid MGID;

	UPROPERTY(BlueprintReadOnly, Category = "Buildings")
	FVector BuildLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Buildings")
	FString BuiltByUPID;

	UPROPERTY(BlueprintReadOnly, Category = "Buildings")
	EMinigameState State = EMinigameState::Idle;

	UPROPERTY(BlueprintReadOnly, Category = "Buildings")
	EMiniGameType Type = EMiniGameType::SignalTower;

	// Type-specific data (e.g. FHabitatData). Empty for types without extra data.
	UPROPERTY(BlueprintReadOnly, Category = "Buildings")
	FInstancedStruct MiniGameData;
};

// Type-specific data for a Habitat building (lives inside FMiniGameRecord::MiniGameData). A habitat
// is a one-way street: built once, claimed by exactly one Signal Tower (permanent), activated once.
USTRUCT(BlueprintType)
struct FHabitatData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Habitat")
	bool bActivated = false;

	// Permanently reserved by a Signal Tower once claimed; never freed, never re-assigned.
	UPROPERTY(BlueprintReadOnly, Category = "Habitat")
	bool bAssignedToSignalTower = false;

	UPROPERTY(BlueprintReadOnly, Category = "Habitat")
	FGuid AssignedTowerMGID;
};
