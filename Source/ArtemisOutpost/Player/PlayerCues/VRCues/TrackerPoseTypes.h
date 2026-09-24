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
