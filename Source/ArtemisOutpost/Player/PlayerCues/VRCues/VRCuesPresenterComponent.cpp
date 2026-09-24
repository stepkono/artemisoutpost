// Fill out your copyright notice in the Description page of Project Settings.

#include "VRCuesPresenterComponent.h"

#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "ArtemisOutpost/GameData/ArtemisPlayerState.h"
#include "Components/SceneComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "IXRTrackingSystem.h"
#include "ArtemisOutpost/Anchors/AnchorsManagerSubsystem.h"
#include "ArtemisOutpost/Player/PawnVR/ACharVR.h"
#include "PlayerCuesVisualizer.h"
#include "AnchorsCueVisualizer.h"

UVRCuesPresenterComponent::UVRCuesPresenterComponent()
{
	// Ticks only while presenting (the Blueprint child's Event Tick does the spawning and filling).
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UVRCuesPresenterComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Spawned actors do not die with their spawner.
	DestroyAllVisualizers();
	Super::EndPlay(EndPlayReason);
}

void UVRCuesPresenterComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	// Runs the Blueprint child's Event Tick (spawning and filling) first, so the summary reflects this frame.
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bLogSummary)
	{
		return;
	}
	SummaryLogTimer += DeltaTime;
	if (SummaryLogTimer < SummaryLogInterval)
	{
		return;
	}
	SummaryLogTimer = 0.0f;
	LogSummary();
}

void UVRCuesPresenterComponent::LogSummary() const
{
	const ACharVR* OwnerVR = Cast<ACharVR>(GetOwner());
	const FVector Pivot    = OwnerVR ? OwnerVR->GetTrackingSpacePivot() : FVector::ZeroVector;
	const FQuat   Tilt     = OwnerVR ? OwnerVR->GetTrackingSpaceTilt().Quaternion() : FQuat::Identity;
	const double  TiltDeg  = FMath::RadiansToDegrees(Tilt.GetAngle());

	// Distances in meters from the tracking pivot (UE units are cm in VR). A ghost many kilometers away means the
	// Blueprint placed an anchor-frame fraction as a world position (missing GetWorldFromAnchorsFrame).
	auto MetersFromPivot = [&Pivot](const FVector& P) { return FVector::Dist(P, Pivot) / 100.0; };

	const TArray<AArtemisPlayerState*> RemotePlayers = GetRemotePlayerStates();

	// How many times do the anchors carry the tilt? The raw (unflattened) table normal is physical up in tracking space,
	// so in world it equals R^k * Z where k is the number of base-orientation applications on the anchor poses.
	// Code reading (OpenXR backend): HMD/controllers get it once (baked into the tracking space), MetaXR's
	// FAnchorsXR::TryGetAnchorTransform applies it a second time on top.
	{
		const UWorld* World = GetWorld();
		const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		UAnchorsManagerSubsystem* AnchorsManager = GameInstance ? GameInstance->GetSubsystem<UAnchorsManagerSubsystem>() : nullptr;
		const TArray<AActor*> Anchors = AnchorsManager ? AnchorsManager->GetAnchors() : TArray<AActor*>();
		if (Anchors.Num() >= 4 && IsValid(Anchors[0]) && IsValid(Anchors[1]) && IsValid(Anchors[3]))
		{
			const FVector A = Anchors[0]->GetActorLocation();
			const FVector TableNormal = FVector::CrossProduct(Anchors[1]->GetActorLocation() - A, Anchors[3]->GetActorLocation() - A).GetSafeNormal();
			auto AngleTo = [&TableNormal](const FVector& Axis) { return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FMath::Abs(FVector::DotProduct(TableNormal, Axis.GetSafeNormal())), 0.0, 1.0))); };
			const double Angle0 = AngleTo(FVector::UpVector);
			const double Angle1 = AngleTo(Tilt.RotateVector(FVector::UpVector));
			const double Angle2 = AngleTo((Tilt * Tilt).RotateVector(FVector::UpVector));
			const int32  Carried = (Angle0 <= Angle1 && Angle0 <= Angle2) ? 0 : (Angle1 <= Angle2 ? 1 : 2);
			UE_LOG(LogTemp, Log, TEXT("[VRCues]   AnchorsTilt | XR=%s | table normal vs Z=%.1f deg, vs R*Z=%.1f deg, vs R^2*Z=%.1f deg -> anchors carry the tilt %dx (HMD/controllers carry it 1x)"),
				(GEngine && GEngine->XRSystem.IsValid()) ? *GEngine->XRSystem->GetSystemName().ToString() : TEXT("none"),
				Angle0, Angle1, Angle2, Carried);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[VRCues] Summary | owner=%s (ACharVR=%d) | tilt=%.2f deg | pivot=%s | remote players=%d | visualizers=%d"),
		*GetNameSafe(GetOwner()), OwnerVR ? 1 : 0, TiltDeg, *Pivot.ToString(), RemotePlayers.Num(), PlayerCuesVisualizers.Num());

	// ---- Anchors ----
	if (IsValid(AnchorsVisualizer))
	{
		const FVector FrameCenter    = AnchorsVisualizer->AnchorsFrame.GetLocation();
		const double  TableDistance  = MetersFromPivot(FrameCenter);
		// How far the tilt moves a point at the table's distance. Tiny values mean "tilted" and "raw" look the same.
		const double  TiltOffsetAtTable = 2.0 * TableDistance * FMath::Sin(FMath::DegreesToRadians(TiltDeg) * 0.5);

		const USceneComponent* VisA = AnchorsVisualizer->GetAnchorComponent(0);
		const FVector VisAWorld = VisA ? VisA->GetComponentLocation() : FVector::ZeroVector;

		FVector RawA0 = FVector::ZeroVector;
		bool bHasRawA0 = false;
		const UWorld* World = GetWorld();
		const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		if (UAnchorsManagerSubsystem* AnchorsManager = GameInstance ? GameInstance->GetSubsystem<UAnchorsManagerSubsystem>() : nullptr)
		{
			const TArray<AActor*> Anchors = AnchorsManager->GetAnchors();
			if (Anchors.Num() > 0 && IsValid(Anchors[0]))
			{
				RawA0 = Anchors[0]->GetActorLocation();
				bHasRawA0 = true;
			}
		}

		UE_LOG(LogTemp, Log, TEXT("[VRCues]   Anchors | visible=%d frame=%d | frameCenter=%s (%.2fm from pivot, tilt moves it ~%.2fm) | AnchorA comp=%s | raw anchor0=%s | comp-raw=%.2fm"),
			AnchorsVisualizer->IsCueVisible() ? 1 : 0, AnchorsVisualizer->bHasAnchorsFrame ? 1 : 0,
			*FrameCenter.ToString(), TableDistance, TiltOffsetAtTable,
			VisA ? *VisAWorld.ToString() : TEXT("none"),
			bHasRawA0 ? *RawA0.ToString() : TEXT("none"),
			(VisA && bHasRawA0) ? FVector::Dist(VisAWorld, RawA0) / 100.0 : -1.0);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[VRCues]   Anchors | no AnchorsVisualizer spawned (AnchorsCuesClass=%s)"), *GetNameSafe(AnchorsCuesClass.Get()));
	}

	// ---- One line per remote player ----
	for (const AArtemisPlayerState* PlayerState : RemotePlayers)
	{
		const int32 PlayerId = PlayerState->GetPlayerId();
		const FTrackerPose& SourcePose = PlayerState->GetTrackerPose();

		const TObjectPtr<APlayerCuesVisualizer>* Found = PlayerCuesVisualizers.Find(PlayerId);
		if (!Found || !IsValid(*Found))
		{
			UE_LOG(LogTemp, Log, TEXT("[VRCues]   PlayerId=%d UPID='%s' ctx=%s hmd=%d | source pose written=%d age=%.2fs | NO visualizer (PlayerCuesClass=%s)"),
				PlayerId, *PlayerState->GetUPID(), *UEnum::GetValueAsString(PlayerState->GetCueState().Context),
				PlayerState->IsHmdWorn() ? 1 : 0, SourcePose.HasBeenWritten() ? 1 : 0, PlayerState->GetTrackerPoseAgeSeconds(),
				*GetNameSafe(PlayerCuesClass.Get()));
			continue;
		}

		const APlayerCuesVisualizer* Visualizer = *Found;
		const USceneComponent* HeadComp  = Visualizer->GetHeadComponent();
		const USceneComponent* LeftComp  = Visualizer->GetLeftHandComponent();
		const USceneComponent* RightComp = Visualizer->GetRightHandComponent();
		const FVector HeadWorld  = HeadComp  ? HeadComp->GetComponentLocation()  : FVector::ZeroVector;
		const FVector LeftWorld  = LeftComp  ? LeftComp->GetComponentLocation()  : FVector::ZeroVector;
		const FVector RightWorld = RightComp ? RightComp->GetComponentLocation() : FVector::ZeroVector;

		// "source" = what the PlayerState holds now, "pulled" = what the visualizer copied (PullPoseFromSource).
		UE_LOG(LogTemp, Log, TEXT("[VRCues]   PlayerId=%d UPID='%s' ctx=%s hmd=%d | source written=%d age=%.2fs | pulled written=%d age=%.2fs active(0.5)=%d visible=%d | tracked L=%d R=%d | frac H=%s | world H=%s (%.2fm) L=%s (%.2fm) R=%s (%.2fm)"),
			PlayerId, *PlayerState->GetUPID(), *UEnum::GetValueAsString(PlayerState->GetCueState().Context), PlayerState->IsHmdWorn() ? 1 : 0,
			SourcePose.HasBeenWritten() ? 1 : 0, PlayerState->GetTrackerPoseAgeSeconds(),
			Visualizer->Pose.HasBeenWritten() ? 1 : 0, Visualizer->GetPoseAgeSeconds(),
			Visualizer->IsSourceActive(0.5f) ? 1 : 0, Visualizer->IsCueVisible() ? 1 : 0,
			Visualizer->Pose.bLeftTracked ? 1 : 0, Visualizer->Pose.bRightTracked ? 1 : 0,
			*Visualizer->Pose.Head.GetLocation().ToString(),
			*HeadWorld.ToString(), MetersFromPivot(HeadWorld),
			*LeftWorld.ToString(), MetersFromPivot(LeftWorld),
			*RightWorld.ToString(), MetersFromPivot(RightWorld));
	}

	// Visualizers whose player is no longer remote (should be pruned) or that lost their source.
	for (const TPair<int32, TObjectPtr<APlayerCuesVisualizer>>& Pair : PlayerCuesVisualizers)
	{
		const bool bStillRemote = RemotePlayers.ContainsByPredicate([&Pair](const AArtemisPlayerState* PS) { return PS->GetPlayerId() == Pair.Key; });
		if (!bStillRemote)
		{
			UE_LOG(LogTemp, Warning, TEXT("[VRCues]   Map entry PlayerId=%d has no matching remote PlayerState (visualizer valid=%d). Is PruneInvalidVisualizers called?"),
				Pair.Key, IsValid(Pair.Value) ? 1 : 0);
		}
	}
}

void UVRCuesPresenterComponent::SetPresenting(bool bInPresenting)
{
	if (bPresenting == bInPresenting)
	{
		return;
	}
	bPresenting = bInPresenting;

	if (!bPresenting)
	{
		DestroyAllVisualizers();
	}
	SetComponentTickEnabled(bPresenting);

	UE_LOG(LogTemp, Log, TEXT("[VRCues] Presenter on %s presenting=%d | remote players=%d"),
		*GetNameSafe(GetOwner()), bPresenting ? 1 : 0, GetRemotePlayerStates().Num());

	OnPresentingChanged(bPresenting);
}

int32 UVRCuesPresenterComponent::PruneInvalidVisualizers()
{
	int32 Removed = 0;
	for (auto It = PlayerCuesVisualizers.CreateIterator(); It; ++It)
	{
		APlayerCuesVisualizer* Visualizer = It.Value();
		if (IsValid(Visualizer) && IsValid(Visualizer->SourcePlayerState))
		{
			continue;
		}

		UE_LOG(LogTemp, Log, TEXT("[VRCues] Pruning visualizer for PlayerId=%d (visualizer valid=%d, source valid=%d)"),
			It.Key(), IsValid(Visualizer) ? 1 : 0, (IsValid(Visualizer) && IsValid(Visualizer->SourcePlayerState)) ? 1 : 0);

		if (IsValid(Visualizer))
		{
			Visualizer->Destroy();
		}
		It.RemoveCurrent();
		++Removed;
	}
	return Removed;
}

void UVRCuesPresenterComponent::DestroyAllVisualizers()
{
	const int32 Count = PlayerCuesVisualizers.Num() + (IsValid(AnchorsVisualizer) ? 1 : 0);

	for (const TPair<int32, TObjectPtr<APlayerCuesVisualizer>>& Pair : PlayerCuesVisualizers)
	{
		if (IsValid(Pair.Value))
		{
			Pair.Value->Destroy();
		}
	}
	PlayerCuesVisualizers.Empty();

	if (IsValid(AnchorsVisualizer))
	{
		AnchorsVisualizer->Destroy();
	}
	AnchorsVisualizer = nullptr;

	if (Count > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[VRCues] Destroyed %d visualizer(s) on %s"), Count, *GetNameSafe(GetOwner()));
	}
}

TArray<AArtemisPlayerState*> UVRCuesPresenterComponent::GetRemotePlayerStates() const
{
	TArray<AArtemisPlayerState*> Result;

	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!GameState)
	{
		if (!bWarnedNoGameState)
		{
			UE_LOG(LogTemp, Warning, TEXT("[VRCues] GetRemotePlayerStates on %s: no GameState yet, returning no players."), *GetNameSafe(GetOwner()));
			bWarnedNoGameState = true;
		}
		return Result;
	}
	bWarnedNoGameState = false;

	// The local player's own PlayerState, from the local controller rather than the pawn, so it is known even
	// in the frame the pawn's possession is still settling.
	const APlayerController* LocalPC = World->GetFirstPlayerController();
	const APlayerState* OwnPlayerState = LocalPC ? LocalPC->GetPlayerState<APlayerState>() : nullptr;
	if (!OwnPlayerState)
	{
		// Without it the local player cannot be excluded and gets a visualizer of its own, which never shows
		// because the owner does not receive its own TrackerPose (COND_SkipOwner).
		if (!bWarnedNoOwnPlayerState)
		{
			UE_LOG(LogTemp, Warning, TEXT("[VRCues] GetRemotePlayerStates on %s: local PlayerState unknown (PC=%s), own player is NOT excluded."),
				*GetNameSafe(GetOwner()), *GetNameSafe(LocalPC));
			bWarnedNoOwnPlayerState = true;
		}
	}
	else
	{
		bWarnedNoOwnPlayerState = false;
	}

	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		AArtemisPlayerState* ArtemisPlayerState = Cast<AArtemisPlayerState>(PlayerState);
		if (!IsValid(ArtemisPlayerState) || ArtemisPlayerState == OwnPlayerState)
		{
			continue;
		}

		// The listen-server host joins as a spectator but still owns a PlayerState in PlayerArray. It has no player
		// pawns, so skip it by the spectator flags and, in case the GameMode does not set them, by the missing pawns.
		const bool bSpectator = ArtemisPlayerState->IsSpectator() || ArtemisPlayerState->IsOnlyASpectator();
		const bool bHasPawns  = ArtemisPlayerState->GetVRChar() || ArtemisPlayerState->GetARPawn();
		if (bSpectator || !bHasPawns)
		{
			if (!LoggedSkippedPlayerIds.Contains(ArtemisPlayerState->GetPlayerId()))
			{
				UE_LOG(LogTemp, Log, TEXT("[VRCues] Skipping PlayerId=%d UPID='%s': spectator=%d hasPawns=%d (host / not a player)."),
					ArtemisPlayerState->GetPlayerId(), *ArtemisPlayerState->GetUPID(), bSpectator ? 1 : 0, bHasPawns ? 1 : 0);
				LoggedSkippedPlayerIds.Add(ArtemisPlayerState->GetPlayerId());
			}
			continue;
		}
		Result.Add(ArtemisPlayerState);
	}
	return Result;
}
