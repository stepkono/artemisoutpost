// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "TrackerPoseTypes.generated.h"

class UWorld;

/**
 * One player's tracked devices, expressed in the shared anchors (table) frame. Positions are fractions of the
 * table edge (see UAnchorsManagerSubsystem::GetRelativeToAnchorsFrame), so the values are independent of every
 * client's WorldToMeters and of the VR base-orientation tilt, which MetaXR applies to the anchors as well.
 *
 * Written on the server by AArtemisPlayerState::ServerSetTrackerPose, replicated to everyone but the owner.
 */
USTRUCT(BlueprintType)
struct ARTEMISOUTPOST_API FTrackerPose
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Tracker Pose")
	FTransform Head = FTransform::Identity;

	UPROPERTY(BlueprintReadWrite, Category = "Tracker Pose")
	FTransform Left = FTransform::Identity;

	UPROPERTY(BlueprintReadWrite, Category = "Tracker Pose")
	FTransform Right = FTransform::Identity;

	UPROPERTY(BlueprintReadWrite, Category = "Tracker Pose")
	bool bLeftTracked = false;

	UPROPERTY(BlueprintReadWrite, Category = "Tracker Pose")
	bool bRightTracked = false;

	// Server world time of the write (AGameStateBase::GetServerWorldTimeSeconds). Negative: never written.
	UPROPERTY(BlueprintReadOnly, Category = "Tracker Pose")
	double ServerWriteTime = -1.0;

	// Returned by GetAgeSeconds for a pose that was never written.
	static constexpr double NeverWrittenAgeSeconds = 1.0e9;

	bool HasBeenWritten() const { return ServerWriteTime >= 0.0; }

	// Seconds since the server wrote this pose, measured in server time, valid on any peer.
	double GetAgeSeconds(const UWorld* World) const;
};

/**
 * The LOCAL player's (the viewer's) own tracked devices, world space, this frame. Not replicated: filled on each
 * client by the presenter from the local pawn and written into the cues (AWorldCue::WriteViewerTrackers), e.g. for
 * proximity effects. An untracked controller reports the tracking origin, so it must be ignored via its flag.
 */
USTRUCT(BlueprintType)
struct ARTEMISOUTPOST_API FCueLocalViewerTrackers
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracker Pose")
	FTransform Head;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracker Pose")
	FTransform Left;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracker Pose")
	FTransform Right;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracker Pose")
	bool bLeftTracked = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracker Pose")
	bool bRightTracked = false;
};

/**
 * Who a world cue is about. Constant for the lifetime of the player's PlayerState (a reconnect gets a new one).
 * Assembled by UWorldCuesPresenterComponent::GetOwnerPlayerIdentity: UPID and number from the PlayerState, the colour
 * from the replicated PlayersRoster entry, which AServerGameMode fills from its PlayerColors (the source of truth).
 */
USTRUCT(BlueprintType)
struct ARTEMISOUTPOST_API FCuePlayerIdentity
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracker Pose")
	FString UPID;

	// 1-based, -1 while unknown.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracker Pose")
	int32 PlayerNumber = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracker Pose")
	FLinearColor Color = FLinearColor::White;
};
