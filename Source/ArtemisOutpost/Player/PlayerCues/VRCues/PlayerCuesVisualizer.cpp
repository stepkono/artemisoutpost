// Fill out your copyright notice in the Description page of Project Settings.

#include "PlayerCuesVisualizer.h"

#include "Components/SceneComponent.h"
#include "ArtemisOutpost/GameData/ArtemisPlayerState.h"

APlayerCuesVisualizer::APlayerCuesVisualizer()
{
	Head = CreateDefaultSubobject<USceneComponent>(TEXT("Head"));
	Head->SetupAttachment(Root);

	LeftHand = CreateDefaultSubobject<USceneComponent>(TEXT("LeftHand"));
	LeftHand->SetupAttachment(Root);

	RightHand = CreateDefaultSubobject<USceneComponent>(TEXT("RightHand"));
	RightHand->SetupAttachment(Root);
}

bool APlayerCuesVisualizer::PullPoseFromSource()
{
	if (!IsValid(SourcePlayerState))
	{
		if (!bLoggedMissingSource)
		{
			UE_LOG(LogTemp, Warning, TEXT("[VRCues] %s (PlayerId=%d): SourcePlayerState invalid, pose not updated. Was it set after spawning?"),
				*GetName(), SourcePlayerId);
			bLoggedMissingSource = true;
		}
		return false;
	}
	bLoggedMissingSource = false;

	const bool bHadPose = Pose.HasBeenWritten();
	Pose = SourcePlayerState->GetTrackerPose();

	if (!Pose.HasBeenWritten())
	{
		if (!bLoggedWaitingForPose)
		{
			UE_LOG(LogTemp, Log, TEXT("[VRCues] %s (PlayerId=%d, UPID '%s'): source has no tracker pose yet (never written on the server, or not replicated here)."),
				*GetName(), SourcePlayerId, *SourcePlayerState->GetUPID());
			bLoggedWaitingForPose = true;
		}
	}
	else if (!bHadPose)
	{
		UE_LOG(LogTemp, Log, TEXT("[VRCues] %s (PlayerId=%d, UPID '%s'): first pose pulled | age=%.2fs H=%s trackedL=%d R=%d"),
			*GetName(), SourcePlayerId, *SourcePlayerState->GetUPID(), GetPoseAgeSeconds(),
			*Pose.Head.GetLocation().ToString(), Pose.bLeftTracked ? 1 : 0, Pose.bRightTracked ? 1 : 0);
		bLoggedWaitingForPose = false;
	}
	return true;
}

bool APlayerCuesVisualizer::IsSourceActive(float MaxPoseAgeSeconds) const
{
	return IsValid(SourcePlayerState)
		&& SourcePlayerState->IsHmdWorn()
		&& Pose.HasBeenWritten()
		&& GetPoseAgeSeconds() <= MaxPoseAgeSeconds;
}

double APlayerCuesVisualizer::GetPoseAgeSeconds() const
{
	return Pose.GetAgeSeconds(GetWorld());
}
