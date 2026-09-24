// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TrackerPoseCacheComponent.generated.h"

class UAnchorsManagerSubsystem;

/**
 * Send-side helper for tracker replication on the locally controlled AR pawn.
 *
 * After a WorldToMeters change the tracked devices (camera, controllers) adopt the new scale one frame
 * before the spatial anchors do. Converting the CURRENT controller pose against the LIVE anchors therefore
 * mixes two WTM spaces during a zoom. The anchors on frame k are in the same WTM space as the controllers on
 * frame k-1, so this component caches the tracker poses at the very end of each frame (TG_PostUpdateWork,
 * which runs after the timer manager) and the replication timer converts last frame's pose against the live
 * anchors on the next frame.
 *
 * Usage from BP_ARPawn: StartCaching on possession (before starting the replication timer), StopCaching on
 * unpossession, and GetCachedPoses inside ReplicateTransforms instead of reading LMC/RMC directly. The Blueprint
 * then converts the returned world poses with UAnchorsManagerSubsystem::GetRelativeToAnchorsFrame.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ARTEMISOUTPOST_API UTrackerPoseCacheComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTrackerPoseCacheComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Sets the tracked sources and starts caching their poses every frame. Call on the owning client only. */
	UFUNCTION(BlueprintCallable, Category = "Tracker Replication")
	void StartCaching(USceneComponent* InCamera, USceneComponent* InLeftController, USceneComponent* InRightController);

	/** Stops caching and invalidates the cached poses. */
	UFUNCTION(BlueprintCallable, Category = "Tracker Replication")
	void StopCaching();

	/**
	 * Last frame's cached poses in WORLD space (not converted). Convert them with GetRelativeToAnchorsFrame on the
	 * same frame you call this, so the pose from frame k-1 meets the anchors of frame k (same WTM space).
	 * Returns false when there is no cached pose yet or the anchors are not ready, in which case nothing should be sent.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tracker Replication")
	bool GetCachedPoses(FTransform& OutCamera, FTransform& OutLeft, FTransform& OutRight);
	
	UFUNCTION(BlueprintPure, Category = "Tracker Replication")
	bool HasPose() const { return bHasPose; }

	UFUNCTION(BlueprintPure, Category = "Tracker Replication")
	bool IsLeftTracked() const { return bLeftTracked; }

	UFUNCTION(BlueprintPure, Category = "Tracker Replication")
	bool IsRightTracked() const { return bRightTracked; }

	UFUNCTION(BlueprintPure, Category = "Tracker Replication")
	FTransform GetLastCameraPose() const { return LastCameraPose; }

	UFUNCTION(BlueprintPure, Category = "Tracker Replication")
	FTransform GetLastLeftPose() const { return LastLeftPose; }

	UFUNCTION(BlueprintPure, Category = "Tracker Replication")
	FTransform GetLastRightPose() const { return LastRightPose; }

	/** Logs every cached frame. Very verbose, lowers the frame rate on device. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracker Replication|Debug")
	bool bLogEveryCache = false;

	/**
	 * Logs every send with the anchor-frame fractions of the returned poses and their change since the previous send.
	 * The fractions are computed for the log only, GetCachedPoses still returns world poses.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracker Replication|Debug")
	bool bLogSends = true;

	/** Warn (every 5 s at most) when the head is farther than this from the table center, in physical meters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracker Replication|Debug", meta = (ClampMin = "0.5"))
	float MaxHeadToTableMeters = 5.0f;

private:
	static bool IsSourceTracked(const USceneComponent* Source);

	UAnchorsManagerSubsystem* ResolveAnchorsManager() const;

	TWeakObjectPtr<USceneComponent> Camera;
	TWeakObjectPtr<USceneComponent> LeftController;
	TWeakObjectPtr<USceneComponent> RightController;

	FTransform LastCameraPose = FTransform::Identity;
	FTransform LastLeftPose   = FTransform::Identity;
	FTransform LastRightPose  = FTransform::Identity;

	bool bHasPose      = false;
	bool bLeftTracked  = false;
	bool bRightTracked = false;

	// GFrameCounter of the frame the cached poses belong to. The send must see (current frame - 1).
	uint64 CachedFrame = 0;

	// Pairing check: with a still controller the sent fraction must stay constant through a zoom.
	// These are anchor-frame fractions (log only), not the world poses GetCachedPoses returns.
	FVector LastSentLeftFraction  = FVector::ZeroVector;
	FVector LastSentRightFraction = FVector::ZeroVector;
	bool bHasLastSent = false;

	// Log-once guards so a missing setup or unready anchors do not spam every frame.
	bool bWarnedMissingTrackers  = false;
	bool bWarnedAnchorsNotReady  = false;

	// Throttle for the head-far-from-table warning (world seconds).
	double LastFarFromTableWarnTime = -1000.0;
};
