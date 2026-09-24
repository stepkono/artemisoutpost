// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "ArtemisOutpost/Miscellaneous/DataTypes.h"
#include "ArtemisOutpost/Player/PlayerCues/PlayerCueTypes.h"
#include "ArtemisOutpost/Player/PlayerCues/VRCues/TrackerPoseTypes.h"
#include "ArtemisPlayerState.generated.h"

class ACharVR;
class APawnAR;
class AMasterRover;
class UPlayerActionProvider;

// Fired on every peer (server: right after a Server* mutator, clients: from OnRep_CueState) whenever
// the cue state changed. The HUD, the moon layers and the pawns' cue managers listen here.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCueStateChanged, const FPlayerCueState&, NewState);

/**
 * Per-player, replicated-to-everyone awareness state.
 *
 * Why a PlayerState and not the controller or the GameMode: a PlayerController exists only on the
 * server and its owning client, so nothing stored there can feed another player's HUD; the GameMode
 * is server-only. The engine spawns one PlayerState per connection, replicates it to all clients and
 * lists it in GameState->PlayerArray, which is exactly the list every HUD needs.
 *
 * Identity: UPID is written server-side from the ?UPID= login option (APawnController::SetUPID), the
 * player number from AServerGameMode's slot. A reconnecting UPID gets a FRESH PlayerState (the engine's
 * own reuse keys on platform net ids, which this project does not use); that is fine because the cue
 * state is live and transient, and the UPID is written again on the new instance.
 *
 * Writers: ONLY the Server* mutators below, all called on the server (from the controller RPCs, the
 * minigame join/leave hooks, and the server pawn's UPlayerCuesManager). Each mutator updates the
 * struct, broadcasts the change locally and emits the matching study event through the
 * UPlayerActionProvider, so the aggregator has one source.
 */
UCLASS()
class ARTEMISOUTPOST_API AArtemisPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AArtemisPlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ---- Identity ----
	UFUNCTION(BlueprintPure, Category = "Player Cues")
	const FString& GetUPID() const { return UPID; }

	UFUNCTION(BlueprintPure, Category = "Player Cues")
	int32 GetPlayerNumber() const { return PlayerNumber; }

	UFUNCTION(BlueprintPure, Category = "Player Cues")
	const FPlayerCueState& GetCueState() const { return CueState; }

	// Server: identity, written from APawnController::SetUPID and AServerGameMode::ProcessNewPlayer.
	void ServerSetUPID(const FString& InUPID);
	void ServerSetPlayerNumber(int32 InNumber);

	// Server: the player's actors, so any peer can resolve "which player does this hit actor belong
	// to" (see FindForActor). Written from APawnController when it learns its pawns.
	void ServerSetPawns(ACharVR* InVRChar, APawnAR* InARPawn, AMasterRover* InRover);

	UFUNCTION(BlueprintPure, Category = "Player Cues")
	ACharVR* GetVRChar() const { return VRChar; }

	UFUNCTION(BlueprintPure, Category = "Player Cues")
	APawnAR* GetARPawn() const { return ARPawn; }

	UFUNCTION(BlueprintPure, Category = "Player Cues")
	AMasterRover* GetMasterRover() const { return MasterRover; }

	// ---- Server mutators (no-ops without authority) ----
	void ServerSetXRMode(EXRMode Mode);
	void ServerSetHmdWorn(bool bWorn);
	void ServerSetTalking(bool bTalking);
	void ServerSetPointing(EPointingHand Hand, bool bPointing);
	void ServerSetPointerTarget(const FPointingTarget& Target);
	void ServerSetGazeTarget(const FPointingTarget& Target);
	void ServerSetToolActivity(EToolActivity Tool);
	void ServerSetMinigame(bool bInMinigame, EMiniGameType Type);
	void ServerSetWalking(bool bWalking);

	// ---- Tracker pose (head + controllers in the anchors frame, drives the VR cue visualizers) ----

	// Server: stores the owning client's poses (anchor-frame fractions: AR sends GetCachedPoses converted with
	// GetRelativeToAnchorsFrame, VR removes its tilt first) and stamps the server time. Called from the
	// Blueprint Server RPC. Kept out of CueState on purpose: it changes at send rate and must not fire
	// OnCueStateChanged / study events. Logs the first write and then a 1 Hz summary with the write rate.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Player Cues|Tracker Pose")
	void ServerSetTrackerPose(const FTransform& Head, const FTransform& Left, const FTransform& Right, bool bLeftTracked, bool bRightTracked);

	UFUNCTION(BlueprintPure, Category = "Player Cues|Tracker Pose")
	const FTrackerPose& GetTrackerPose() const { return TrackerPose; }

	// Seconds since the server last wrote the tracker pose (server time, valid on any peer).
	UFUNCTION(BlueprintPure, Category = "Player Cues|Tracker Pose")
	double GetTrackerPoseAgeSeconds() const;

	// Shortcut for CueState.bHmdWorn (written by ServerSetHmdWorn from the doff/don detection).
	UFUNCTION(BlueprintPure, Category = "Player Cues")
	bool IsHmdWorn() const { return CueState.bHmdWorn; }
	
	UFUNCTION(BlueprintCallable, Category = "VR Char Rig")
	EXRMode GetXRMode() const { return LastXRMode; }

	// ---- Lookups (any peer) ----

	// The PlayerState carrying this UPID, or null.
	static AArtemisPlayerState* FindByUPID(const UWorld* World, const FString& InUPID);

	// The PlayerState whose VRChar / ARPawn / MasterRover is this actor (puppets are unwrapped by the
	// caller). Null when the actor belongs to no player.
	static AArtemisPlayerState* FindForActor(const UWorld* World, const AActor* Actor);

	// Blueprint access to FindForActor. Unlike APawn::PlayerState (only set while the pawn is POSSESSED, so null on
	// a remote player's parked VR character or unused AR pawn), this works on every peer regardless of possession,
	// because it matches the replicated pawn references written by ServerSetPawns.
	UFUNCTION(BlueprintPure, Category = "Player Cues", meta = (WorldContext = "WorldContextObject", DisplayName = "Find Player State For Actor"))
	static AArtemisPlayerState* FindPlayerStateForActor(const UObject* WorldContextObject, const AActor* Actor);

	UPROPERTY(BlueprintAssignable, Category = "Player Cues")
	FOnCueStateChanged OnCueStateChanged;

private:
	UFUNCTION()
	void OnRep_CueState();

	// Client: throttled log proving the pose reaches this peer (rate, age, values).
	UFUNCTION()
	void OnRep_TrackerPose();

	// Recompute Context from bHmdWorn + LastXRMode and emit ContextChanged if it flipped.
	void RecomputeContext();

	// Broadcast + emit ActivityChanged if the derived activity differs from the last emitted one.
	void CommitChange();

	void EmitEvent(EGameEventType Event);

	UPlayerActionProvider* GetProvider() const;

	UPROPERTY(Replicated)
	FString UPID;

	UPROPERTY(Replicated)
	int32 PlayerNumber = -1;

	UPROPERTY(Replicated)
	TObjectPtr<ACharVR> VRChar;

	UPROPERTY(Replicated)
	TObjectPtr<APawnAR> ARPawn;

	UPROPERTY(Replicated)
	TObjectPtr<AMasterRover> MasterRover;

	UPROPERTY(ReplicatedUsing = OnRep_CueState)
	FPlayerCueState CueState;

	// Replicated to everyone except the owner, who never visualizes itself.
	UPROPERTY(ReplicatedUsing = OnRep_TrackerPose)
	FTrackerPose TrackerPose;

	// ---- Tracker pose logging (server: writes, client: receives), throttled to TrackerPoseLogInterval ----
	bool   bLoggedFirstTrackerPose       = false;
	bool   bWarnedTrackerPoseNoAuthority = false;
	double LastTrackerPoseLogTime        = -1000.0;
	int32  TrackerPoseWritesSinceLog     = 0;
	double LastTrackerPoseRepLogTime     = -1000.0;
	int32  TrackerPoseRepsSinceLog       = 0;
	double LastTrackerPoseSanityWarnTime = -1000.0;
	static constexpr double TrackerPoseLogInterval = 1.0;

	// Server-only bookkeeping.
	EXRMode LastXRMode = EXRMode::AR;
	EPlayerActivity LastEmittedActivity = EPlayerActivity::Idle;
	double LastPointingUpdateTime = -1000.0;

	// Minimum seconds between two PlayerPointingUpdate events for a target that only MOVED.
	static constexpr double PointingUpdateInterval = 0.2;
};
