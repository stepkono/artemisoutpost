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

bool UTrackerPoseCacheComponent::GetCachedPosesInAnchorsFrame(FTransform& OutCamera, FTransform& OutLeft, FTransform& OutRight)
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

	OutCamera = AnchorsManager->GetRelativeToAnchorsFrame(LastCameraPose);
	OutLeft   = AnchorsManager->GetRelativeToAnchorsFrame(LastLeftPose);
	OutRight  = AnchorsManager->GetRelativeToAnchorsFrame(LastRightPose);

	if (bLogSends)
	{
		// Pairing check: hold a controller still and zoom. Its fraction change (d) must stay at tracking-noise level.
		// Fractions are in table-edge units, so d = 0.01 is 1 % of the table edge.
		const double LeftDelta  = bHasLastSent ? FVector::Dist(OutLeft.GetLocation(),  LastSentLeft)  : 0.0;
		const double RightDelta = bHasLastSent ? FVector::Dist(OutRight.GetLocation(), LastSentRight) : 0.0;

		UE_LOG(LogTemp, Log, TEXT("[TrackerPoseCache] SEND frame=%llu age=%llu WTM=%.2f | L=%s d=%.4f tracked=%d | R=%s d=%.4f tracked=%d"),
			GFrameCounter, Age, UHeadMountedDisplayFunctionLibrary::GetWorldToMetersScale(this),
			*OutLeft.GetLocation().ToString(),  LeftDelta,  bLeftTracked  ? 1 : 0,
			*OutRight.GetLocation().ToString(), RightDelta, bRightTracked ? 1 : 0);
	}

	LastSentLeft  = OutLeft.GetLocation();
	LastSentRight = OutRight.GetLocation();
	bHasLastSent  = true;

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
