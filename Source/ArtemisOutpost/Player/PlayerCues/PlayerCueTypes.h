// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArtemisOutpost/MiniGames/General/MinigameTypes.h"
#include "PlayerCueTypes.generated.h"

// The context a player is currently in. "R" = Reality: the HMD is doffed, we cannot know whether the
// person sits at the PC or just took the headset off, so we do not claim more than "not in XR".
UENUM(BlueprintType)
enum class EPlayerContext : uint8
{
	AR UMETA(DisplayName = "AR"),
	VR UMETA(DisplayName = "VR"),
	R  UMETA(DisplayName = "R"),
};

// Which hand-held tool a player currently has drawn. Maps 1:1 onto the existing tools: the building
// tool and the three EScanMode variants of the resource tool.
UENUM(BlueprintType)
enum class EToolActivity : uint8
{
	None            UMETA(DisplayName = "None"),
	ScanArea        UMETA(DisplayName = "Umgebung scannen"),
	ScanSurface     UMETA(DisplayName = "Oberfläche scannen"),
	CollectResource UMETA(DisplayName = "Ressource abbauen"),
	Building        UMETA(DisplayName = "Bauen"),
};

// The single activity shown in the HUD row. Derived from the raw facts in FPlayerCueState by
// FPlayerCueState::GetActivity() with the agreed priority:
// InMinigame > Pointing > UsingTool > Walking > Idle.
UENUM(BlueprintType)
enum class EPlayerActivity : uint8
{
	Idle       UMETA(DisplayName = "Idle"),
	Walking    UMETA(DisplayName = "Geht"),
	UsingTool  UMETA(DisplayName = "Werkzeug"),
	Pointing   UMETA(DisplayName = "Zeigt"),
	InMinigame UMETA(DisplayName = "Minigame"),
};

// What a pointer or gaze ray resolved to. The raw hit actor differs per context (VR hits the real
// actors, AR hits the puppets on the table moon), so the hit is resolved to this abstract target on
// the client that traced it and only the abstract form travels.
UENUM(BlueprintType)
enum class EPointingTargetKind : uint8
{
	None     UMETA(DisplayName = "None"),
	Surface  UMETA(DisplayName = "Oberfläche"),
	Minigame UMETA(DisplayName = "Gebäude"),
	Player   UMETA(DisplayName = "Spieler"),
	Rover    UMETA(DisplayName = "Rover"),
};

UENUM(BlueprintType)
enum class EPointingHand : uint8
{
	Left  UMETA(DisplayName = "Left"),
	Right UMETA(DisplayName = "Right"),
};

// Context-free description of a pointing / gaze hit.
USTRUCT(BlueprintType)
struct ARTEMISOUTPOST_API FPointingTarget
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Player Cues")
	EPointingTargetKind Kind = EPointingTargetKind::None;

	// Longitude / latitude / height on the moon. Filled for every kind except None, so a beacon can
	// always be placed. Converted with the tracing client's own georeference (AR or VR moon).
	UPROPERTY(BlueprintReadOnly, Category = "Player Cues")
	FVector GeoHit = FVector::ZeroVector;

	// Kind == Minigame.
	UPROPERTY(BlueprintReadOnly, Category = "Player Cues")
	FGuid MinigameMGID;

	UPROPERTY(BlueprintReadOnly, Category = "Player Cues")
	EMiniGameType MinigameType = EMiniGameType::SignalTower;

	// Kind == Player or Rover: the player the hit actor belongs to. Number is carried for display so
	// the HUD does not need a lookup.
	UPROPERTY(BlueprintReadOnly, Category = "Player Cues")
	FString TargetUPID;

	UPROPERTY(BlueprintReadOnly, Category = "Player Cues")
	int32 TargetPlayerNumber = -1;

	// True when the two targets name the same THING (kind + id), ignoring the exact geo position.
	bool IsSameTarget(const FPointingTarget& Other) const
	{
		if (Kind != Other.Kind)
		{
			return false;
		}
		switch (Kind)
		{
		case EPointingTargetKind::Minigame: return MinigameMGID == Other.MinigameMGID;
		case EPointingTargetKind::Player:
		case EPointingTargetKind::Rover:    return TargetUPID == Other.TargetUPID;
		default:                            return true;
		}
	}

	bool IsValid() const { return Kind != EPointingTargetKind::None; }
};

// Everything other players may know about one player. ONE replicated struct on AArtemisPlayerState
// (single OnRep, single delegate); small enough that whole-struct replication at pointer rate is a
// non-issue for four players. Stores raw facts, the HUD activity is derived (GetActivity).
USTRUCT(BlueprintType)
struct ARTEMISOUTPOST_API FPlayerCueState
{
	GENERATED_BODY()

	// ---- Context ----
	UPROPERTY(BlueprintReadOnly, Category = "Player Cues")
	EPlayerContext Context = EPlayerContext::AR;

	// Assumed worn until the client reports a doff (see APawnController::ServerReportHmdState).
	UPROPERTY(BlueprintReadOnly, Category = "Player Cues")
	bool bHmdWorn = true;

	// ---- Activity facts ----
	UPROPERTY(BlueprintReadOnly, Category = "Player Cues")
	bool bInMinigame = false;

	UPROPERTY(BlueprintReadOnly, Category = "Player Cues")
	EMiniGameType MinigameType = EMiniGameType::SignalTower;

	UPROPERTY(BlueprintReadOnly, Category = "Player Cues")
	EToolActivity ToolActivity = EToolActivity::None;

	UPROPERTY(BlueprintReadOnly, Category = "Player Cues")
	bool bWalking = false;

	// ---- Walkie-talkie ----
	UPROPERTY(BlueprintReadOnly, Category = "Player Cues")
	bool bTalking = false;

	// ---- Pointing ----
	UPROPERTY(BlueprintReadOnly, Category = "Player Cues")
	bool bPointing = false;

	UPROPERTY(BlueprintReadOnly, Category = "Player Cues")
	EPointingHand PointingHand = EPointingHand::Left;

	UPROPERTY(BlueprintReadOnly, Category = "Player Cues")
	FPointingTarget PointerTarget;

	// ---- Gaze ----
	UPROPERTY(BlueprintReadOnly, Category = "Player Cues")
	FPointingTarget GazeTarget;

	// Priority-derived activity for the HUD column.
	EPlayerActivity GetActivity() const
	{
		if (bInMinigame)                              return EPlayerActivity::InMinigame;
		if (bPointing && PointerTarget.IsValid())     return EPlayerActivity::Pointing;
		if (ToolActivity != EToolActivity::None)      return EPlayerActivity::UsingTool;
		if (bWalking)                                 return EPlayerActivity::Walking;
		return EPlayerActivity::Idle;
	}
};
