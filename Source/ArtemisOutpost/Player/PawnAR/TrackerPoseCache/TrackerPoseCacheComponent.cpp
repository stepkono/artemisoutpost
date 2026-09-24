// Fill out your copyright notice in the Description page of Project Settings.

#include "TrackerPoseCacheComponent.h"

#include "HeadMountedDisplayFunctionLibrary.h"
#include "MotionControllerComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "ArtemisOutpost/Anchors/AnchorsManagerSubsystem.h"

UTrackerPoseCacheComponent::UTrackerPoseCacheComponent()
{
	// Cache at the very end of the frame: after every pose update of this frame and after the timer manager,
	// so the replication timer on frame k always reads the complete pose of frame k-1.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UTrackerPoseCacheComponent::StartCaching(USceneComponent* InCamera, USceneComponent* InLeftController, USceneComponent* InRightController)
{
	Camera          = InCamera;
	LeftController  = InLeftController;
	RightController = InRightController;

	bHasPose     = false;
	bHasLastSent = false;
	CachedFrame  = 0;
	bWarnedMissingTrackers = false;
	bWarnedAnchorsNotReady = false;

	SetComponentTickEnabled(true);

	UE_LOG(LogTemp, Log, TEXT("[TrackerPoseCache] StartCaching on %s | Camera=%s Left=%s (motion controller=%d) Right=%s (motion controller=%d) | frame=%llu"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(InCamera),
		*GetNameSafe(InLeftController),  Cast<UMotionControllerComponent>(InLeftController)  ? 1 : 0,
		*GetNameSafe(InRightController), Cast<UMotionControllerComponent>(InRightController) ? 1 : 0,
		GFrameCounter);
}

void UTrackerPoseCacheComponent::StopCaching()
{
	SetComponentTickEnabled(false);
	bHasPose     = false;
	bHasLastSent = false;

	UE_LOG(LogTemp, Log, TEXT("[TrackerPoseCache] StopCaching on %s | frame=%llu"), *GetNameSafe(GetOwner()), GFrameCounter);
}

void UTrackerPoseCacheComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const USceneComponent* CameraComp = Camera.Get();
	const USceneComponent* LeftComp   = LeftController.Get();
	const USceneComponent* RightComp  = RightController.Get();

	if (!CameraComp || !LeftComp || !RightComp)
	{
		if (!bWarnedMissingTrackers)
		{
			UE_LOG(LogTemp, Warning, TEXT("[TrackerPoseCache] %s: tracker references missing (Camera=%d Left=%d Right=%d). Call StartCaching with valid components."),
				*GetNameSafe(GetOwner()), CameraComp ? 1 : 0, LeftComp ? 1 : 0, RightComp ? 1 : 0);
			bWarnedMissingTrackers = true;
		}
		bHasPose = false;
		return;
	}

	LastCameraPose = CameraComp->GetComponentTransform();
	LastLeftPose   = LeftComp->GetComponentTransform();
	LastRightPose  = RightComp->GetComponentTransform();

	// An untracked motion controller falls back to the tracking origin, log the transitions so those
	// samples can be told apart from real poses.
	const bool bNowLeftTracked  = IsSourceTracked(LeftComp);
	const bool bNowRightTracked = IsSourceTracked(RightComp);
	if (bNowLeftTracked != bLeftTracked || bNowRightTracked != bRightTracked || !bHasPose)
	{
		UE_LOG(LogTemp, Log, TEXT("[TrackerPoseCache] Tracking state frame=%llu | Left tracked=%d Right tracked=%d"),
			GFrameCounter, bNowLeftTracked ? 1 : 0, bNowRightTracked ? 1 : 0);
	}
	bLeftTracked  = bNowLeftTracked;
	bRightTracked = bNowRightTracked;

	CachedFrame = GFrameCounter;
	bHasPose = true;

	if (bLogEveryCache)
	{
		UE_LOG(LogTemp, Log, TEXT("[TrackerPoseCache] CACHE frame=%llu WTM=%.2f | L=%s R=%s Cam=%s"),
			CachedFrame, UHeadMountedDisplayFunctionLibrary::GetWorldToMetersScale(this),
			*LastLeftPose.GetLocation().ToString(), *LastRightPose.GetLocation().ToString(), *LastCameraPose.GetLocation().ToString());
	}
}

bool UTrackerPoseCacheComponent::GetCachedPoses(FTransform& OutCamera, FTransform& OutLeft, FTransform& OutRight)
{
	OutCamera = FTransform::Identity;
	OutLeft   = FTransform::Identity;
	OutRight  = FTransform::Identity;

	if (!bHasPose)
	{
		return false;
	}

	const UAnchorsManagerSubsystem* AnchorsManager = ResolveAnchorsManager();
	if (!AnchorsManager || !AnchorsManager->HasValidAnchorsFrame())
	{
		if (!bWarnedAnchorsNotReady)
		{
			UE_LOG(LogTemp, Warning, TEXT("[TrackerPoseCache] SEND frame=%llu skipped: anchors frame not ready yet."), GFrameCounter);
			bWarnedAnchorsNotReady = true;
		}
		return false;
	}
	if (bWarnedAnchorsNotReady)
	{
		UE_LOG(LogTemp, Log, TEXT("[TrackerPoseCache] SEND frame=%llu anchors frame ready, sending resumes."), GFrameCounter);
		bWarnedAnchorsNotReady = false;
	}

	// Order check. The timer manager runs before TG_PostUpdateWork, so the newest cached pose must belong to the
	// previous frame. Age 0 means the cache ticked before the timer this frame, age > 1 means a frame was skipped.
	const uint64 Age = GFrameCounter - CachedFrame;
	if (Age != 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TrackerPoseCache] SEND frame=%llu reads a pose cached on frame %llu (age=%llu, expected 1). Controller and anchor WTM spaces may be mismatched."), GFrameCounter, CachedFrame, Age);
	}

	// World poses. The caller converts them with GetRelativeToAnchorsFrame on this same frame.
	OutCamera = LastCameraPose;
	OutLeft   = LastLeftPose;
	OutRight  = LastRightPose;

	// Sanity check: the head should be within a few meters of the table. Far away means this device's anchors are not
	// at the physical table in its tracking space (not located, stale, or from a different tracking space).
	FTransform Frame;
	double HeadToTableMeters = -1.0;
	if (AnchorsManager->GetAnchorsFrameTransform(Frame))
	{
		const double WorldToMeters = FMath::Max(1.0, (double)UHeadMountedDisplayFunctionLibrary::GetWorldToMetersScale(this));
		HeadToTableMeters = FVector::Dist(LastCameraPose.GetLocation(), Frame.GetLocation()) / WorldToMeters;

		const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
		if (HeadToTableMeters > MaxHeadToTableMeters && Now - LastFarFromTableWarnTime >= 5.0)
		{
			UE_LOG(LogTemp, Warning, TEXT("[TrackerPoseCache] %s: head is %.1f m from the anchors frame center (table). The anchors are probably not at the physical table on this device, every sent pose will be wrong. Camera=%s TableCenter=%s"),
				*GetNameSafe(GetOwner()), HeadToTableMeters, *LastCameraPose.GetLocation().ToString(), *Frame.GetLocation().ToString());
			LastFarFromTableWarnTime = Now;
		}
	}

	if (bLogSends)
	{
		// Pairing check on the anchor-frame fractions, computed here for the log only. World positions change during a
		// zoom even with a still hand (the tracked space scales), so the check is meaningless on world values.
		// Hold a controller still and zoom: its fraction change (d) must stay at tracking-noise level.
		// Fractions are in table-edge units, so d = 0.01 is 1 % of the table edge.
		const FVector LeftFraction  = AnchorsManager->GetRelativeToAnchorsFrame(LastLeftPose).GetLocation();
		const FVector RightFraction = AnchorsManager->GetRelativeToAnchorsFrame(LastRightPose).GetLocation();

		const double LeftDelta  = bHasLastSent ? FVector::Dist(LeftFraction,  LastSentLeftFraction)  : 0.0;
		const double RightDelta = bHasLastSent ? FVector::Dist(RightFraction, LastSentRightFraction) : 0.0;

		UE_LOG(LogTemp, Log, TEXT("[TrackerPoseCache] SEND frame=%llu age=%llu WTM=%.2f | head-table=%.2fm | Lfrac=%s d=%.4f tracked=%d | Rfrac=%s d=%.4f tracked=%d"),
			GFrameCounter, Age, UHeadMountedDisplayFunctionLibrary::GetWorldToMetersScale(this), HeadToTableMeters,
			*LeftFraction.ToString(),  LeftDelta,  bLeftTracked  ? 1 : 0,
			*RightFraction.ToString(), RightDelta, bRightTracked ? 1 : 0);

		LastSentLeftFraction  = LeftFraction;
		LastSentRightFraction = RightFraction;
		bHasLastSent = true;
	}

	return true;
}



bool UTrackerPoseCacheComponent::IsSourceTracked(const USceneComponent* Source)
{
	if (const UMotionControllerComponent* MotionController = Cast<UMotionControllerComponent>(Source))
	{
		return MotionController->IsTracked();
	}
	// Not a motion controller (e.g. the camera): treat as always tracked.
	return Source != nullptr;
}

UAnchorsManagerSubsystem* UTrackerPoseCacheComponent::ResolveAnchorsManager() const
{
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UAnchorsManagerSubsystem>() : nullptr;
}
