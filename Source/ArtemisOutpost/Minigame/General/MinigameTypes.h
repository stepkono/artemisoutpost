// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MinigameTypes.generated.h"

// A placeable outpost building. Each maps to a concrete AMinigameActor subclass via the
// BuildingClasses map on ABuildingTool (Antenna -> ASignalTower today). Named with the Outpost
// prefix because plain "EBuildingType" collides with an engine/plugin enum.
UENUM(BlueprintType)
enum class EOutpostBuildingType : uint8
{
	Habitat    UMETA(DisplayName = "Habitat"),
	SolarPanel UMETA(DisplayName = "Solar Panel"),
	SignalTower    UMETA(DisplayName = "SignalTower")
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
struct FAxisState
{
	GENERATED_BODY()

	// Current angle in degrees (0..360).
	UPROPERTY(BlueprintReadOnly, Category = "Minigame")
	float Value = 0.0f;

	// Target angle the participant rotates to. Set server-side on start.
	UPROPERTY(BlueprintReadOnly, Category = "Minigame")
	float TargetValue = 0.0f;

	// Seconds this axis has continuously been within tolerance (server-driven dwell).
	UPROPERTY(BlueprintReadOnly, Category = "Minigame")
	float InToleranceTime = 0.0f;

	// UPID that exclusively controls this axis. Empty = any participant may (the solo case).
	UPROPERTY(BlueprintReadOnly, Category = "Minigame")
	FString OwnerUPID;
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
