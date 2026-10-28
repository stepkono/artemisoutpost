// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ArtemisOutpost/Miscellaneous/DataTypes.h"
#include "TrackerPoseTypes.h"
#include "WorldCuesPresenterComponent.generated.h"

class AArtemisPlayerState;
class AWorldCue;
class AVRCuesPlayer;
class AARCuesPlayer;
class AVRCuesAnchors;

/**
 * Presents the world cues of ONE player (the player of the pawn this component sits on) to the LOCAL viewer.
 * Added in Blueprint on both BP_ARPawn and BP_VRChar, so every pawn instance on every machine has one.
 *
 * It never looks at its owner's context. Only two inputs decide:
 *  - Is the owner pawn possessed? GetOwnerPawnPlayerState is the replicated APawn::PlayerState, valid only while
 *    possessed, so exactly one of a player's two pawns presents. The other one keeps its cues off.
 *  - The local viewer's context (GetLocalViewerContext): which cues are relevant (AR cues in AR, VR cues in VR).
 * The decision logic runs in the Blueprint child's Event Tick, this class provides the building blocks. The tick
 * always runs, so cues switched off (no tick) can always be switched back on from here.
 *
 * Data: the cues are vessels. The Blueprint writes the owner's tracker pose and the viewer's trackers into them every
 * tick (WritePoseToCues, WritePlayerCueStateToCues, WritePlayerIdentityToCues, WriteViewerTrackersToCues), the cues
 * never fetch anything themselves.
 *
 * Lifecycle: the cues are spawned through SpawnCue and die with this component (EndPlay).
 */
UCLASS(Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ARTEMISOUTPOST_API UWorldCuesPresenterComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWorldCuesPresenterComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Runs the Blueprint child's Event Tick first, then the throttled diagnostics summary.
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ---- Owner ----

	// The owner pawn's APawn::PlayerState. Replicated to every machine and only set while the pawn is possessed,
	// so it is null on the parked pawn of a player and on the pawns of a disconnected player.
	UFUNCTION(BlueprintPure, Category = "World Cues")
	AArtemisPlayerState* GetOwnerPawnPlayerState() const;

	// UPID and number from the owner PlayerState, colour from its PlayersRoster entry (assigned by AServerGameMode).
	// False while incomplete: owner not possessed, UPID not replicated yet or no roster entry yet. OutIdentity then
	// holds whatever is known so far.
	UFUNCTION(BlueprintPure, Category = "World Cues")
	bool GetOwnerPlayerIdentity(FCuePlayerIdentity& OutIdentity) const;

	// ---- Local viewer ----

	// The local player's PlayerState (from the local PlayerController, independent of possession).
	UFUNCTION(BlueprintPure, Category = "World Cues")
	AArtemisPlayerState* GetLocalViewerPlayerState() const;

	// The local player's replicated context (CueState.Context, not the server-only GetXRMode). False while unknown.
	UFUNCTION(BlueprintPure, Category = "World Cues")
	bool GetLocalViewerContext(ERVMode& OutContext) const;

	// ---- Cues ----

	// Spawns a client-local cue, inactive, and registers it in Cues.
	UFUNCTION(BlueprintCallable, Category = "World Cues", meta = (DeterminesOutputType = "CueClass"))
	AWorldCue* SpawnCue(TSubclassOf<AWorldCue> CueClass);

	// Writes Pose into every registered cue (inactive ones included, so they start with fresh data).
	UFUNCTION(BlueprintCallable, Category = "World Cues")
	void WritePoseToCues(const FTrackerPose& Pose);

	// Writes the owner's cue state (OwnerPlayerState -> GetCueState) into every registered cue.
	UFUNCTION(BlueprintCallable, Category = "World Cues")
	void WritePlayerCueStateToCues(const FPlayerCueState& CueState);

	// Writes the owner's identity (GetOwnerPlayerIdentity) into every registered cue. Cheap enough to call every tick,
	// which also heals a UPID or roster entry that replicated after the cues were spawned.
	UFUNCTION(BlueprintCallable, Category = "World Cues")
	void WritePlayerIdentityToCues(const FCuePlayerIdentity& Identity);

	// Writes the local viewer's trackers into every registered cue.
	UFUNCTION(BlueprintCallable, Category = "World Cues")
	void WriteViewerTrackersToCues(const FCueLocalViewerTrackers& Trackers);

	// Toggles every registered cue whose GetLocalViewerCueContext() equals CueContext.
	UFUNCTION(BlueprintCallable, Category = "World Cues")
	void ToggleCuesOfContext(ERVMode CueContext, bool bActivate);

	UFUNCTION(BlueprintCallable, Category = "World Cues")
	void ToggleAllCues(bool bActivate);

	UFUNCTION(BlueprintCallable, Category = "World Cues")
	void DestroyAllCues();

	UPROPERTY(Transient, BlueprintReadOnly, Category = "World Cues")
	TArray<TObjectPtr<AWorldCue>> Cues;

	// ---- Cue classes (set in the Blueprint defaults) ----

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Cues|Classes")
	TSubclassOf<AVRCuesPlayer> VRCuesPlayerClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Cues|Classes")
	TSubclassOf<AARCuesPlayer> ARCuesPlayerClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Cues|Classes")
	TSubclassOf<AVRCuesAnchors> VRCuesAnchorsClass;

	// ---- Debug ----

	// While the owner is possessed or cues exist: one line per presenter (owner, viewer context, cue states).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Cues|Debug")
	bool bLogSummary = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Cues|Debug", meta = (ClampMin = "0.1"))
	float SummaryLogInterval = 1.0f;

private:
	void LogSummary() const;
	void RemoveInvalidCues();

	float SummaryLogTimer = 0.0f;
};
