// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArtemisOutpost/MiniGames/General/GameInstance/MinigameActor.h"
#include "ArtemisOutpost/MiniGames/General/MinigameTypes.h"
#include "CoupledAxisMinigameActor.generated.h"

// Broadcast on every peer whenever the axes change (value, target, or ownership). Subscribers: the
// per-player controller (forwards to the screen UI) and the actor BP (meshes/beams). Fires on the
// server directly from the mechanics and on remote clients from OnRep.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAxesUpdated, const TArray<FAxisData>&, Axes);

// Reusable "two people, two locked degrees of freedom" minigame actor. Signal Tower (Earth +
// Habitat) and Habitat (Pitch + Roll) both derive from this. It OWNS the replicated Axes, runs the
// mechanics (claim/release/rotate + dwell bookkeeping) and holds the coupled-axis rules itself as
// config properties + overridable virtuals — no separate rules component.
//
// Ownership is EXPLICIT: a participant claims an axis (the selection screen), owns exactly one at a
// time, and only the owner may rotate it. A second participant can only claim a still-free axis, so
// the two are coupled one-per-person (§8.6/§8.7). Solo: claim one, align it, claim the other (the
// first keeps its value) — completion checks all axes regardless of owner.
//
// Two policies differ per game and are therefore virtuals with tower-preserving defaults:
//  - WHEN rotation applies: IsRotationOpen. The tower is always open. The Habitat is open only while
//    both axes are owned (both players on the rotation screen).
//  - HOW the task completes: SolvesAxisOnLeave + EvaluateCompletion. The tower judges an axis when
//    its owner leaves and completes once every axis was left aligned. The Habitat completes from the
//    server tick once every axis has dwelled in tolerance simultaneously.
UCLASS(Abstract)
class ARTEMISOUTPOST_API ACoupledAxisMinigameActor : public AMinigameActor
{
	GENERATED_BODY()

public:
	ACoupledAxisMinigameActor();
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(BlueprintAssignable, Category = "Minigame")
	FOnAxesUpdated OnAxesUpdated;

	UFUNCTION(BlueprintPure, Category = "Minigame")
	const TArray<FAxisData>& GetAxes() const;

	UFUNCTION(BlueprintPure, Category = "Minigame")
	int32 GetNumAxes() const;

	// 0..1 dwell fraction of an axis, for UI / awareness cues.
	UFUNCTION(BlueprintPure, Category = "Minigame")
	float GetAxisProgress(int32 AxisIndex) const;

	// Whether UPID owns (may rotate) this axis.
	UFUNCTION(BlueprintPure, Category = "Minigame")
	bool CanControlAxis(const FString& UPID, int32 AxisIndex) const;

	// Index of the axis UPID currently owns, or INDEX_NONE. Used client-side to resolve which axis a
	// rotate/release intent targets without the input layer having to know axis indices.
	UFUNCTION(BlueprintPure, Category = "Minigame")
	int32 GetAxisOwnedBy(const FString& UPID) const;

	// Whether an axis is currently within tolerance (green "levelled" feedback).
	UFUNCTION(BlueprintPure, Category = "Minigame")
	bool IsAxisAligned(int32 AxisIndex) const;

	// Permanently finished. Never claimable again.
	UFUNCTION(BlueprintPure, Category = "Minigame")
	bool IsAxisSolved(int32 AxisIndex) const;

	// Every axis solved, i.e. the whole task is done and the minigame is no longer playable.
	UFUNCTION(BlueprintPure, Category = "Minigame")
	bool AreAllAxesSolved() const;

	// Number of axes that currently have an owner. Two owned axes on a two-axis game means both
	// participants are on their rotation screen (the View derives that page from ownership).
	UFUNCTION(BlueprintPure, Category = "Minigame")
	int32 GetOwnedAxisCount() const;

	// Dwell duration, pushed to the screen UI to draw a progress ring from InToleranceTime.
	UFUNCTION(BlueprintPure, Category = "Minigame")
	float GetDwellSeconds() const { return DwellSeconds; }

	// Target angle (deg) an axis must reach. NOT stored in FAxisData — it is a fixed per-game
	// property (e.g. a SignalTower's Earth/Habitat targets), so subclasses provide it here. Used by
	// completion + alignment, and readable by UI. Base returns 0.
	UFUNCTION(BlueprintPure, Category = "Minigame")
	virtual float GetAxisTarget(int32 AxisIndex) const;

	// Whether rotation input is currently applied to the axes. Works on every peer from replicated
	// data, so the client-side gesture can stop sending while closed and the server refuses what still
	// arrives. OutReason is for the refusal log. Base: always open (the Signal Tower).
	// Also gates the dwell timers: while closed they are held at zero, so time spent alone never
	// counts toward a "hold steady" completion.
	UFUNCTION(BlueprintPure, Category = "Minigame")
	virtual bool IsRotationOpen(FString& OutReason) const;

	// Client-side: maps the fired InputAction to an intent (via InputActionMap) and interprets it —
	// Rotate turns the axis this player owns (joystick angle -> delta degrees), Release lets it go.
	// The analog value is read on demand via GetLocalActionValue. Shared by every coupled-axis game.
	virtual void ProcessInput(UInputAction* InputAction, EInputActionType TriggerEvent) override;

protected:
	virtual void BeginPlay() override;
	virtual void OnStart() override;
	virtual void OnAbort() override;
	virtual void ApplyInput(const FString& UPID, const FMinigameInput& Input) override;
	virtual void OnParticipantLeft(const FString& UPID) override;
	virtual void SyncPuppet() override;
	virtual TSharedRef<class FJsonObject> BuildSnapshot() const override;

	// --- Coupled-axis rules (overridable per game) ---

	// Number of controllable degrees of freedom.
	virtual int32 GetAxisCount() const;

	// Whether an axis is judged (and marked solved when aligned) the moment its owner leaves the
	// minigame. True for the tower, where leaving IS the commit. A game that completes from the tick
	// (Habitat) returns false so a player stepping out never freezes half the task as solved.
	virtual bool SolvesAxisOnLeave() const;

	// Server tick hook, called while Active and IsRotationOpen, right after the dwell timers were
	// refreshed. A game whose completion is time-based decides here and calls OnComplete. Base: empty,
	// the tower completes on leave instead.
	virtual void EvaluateCompletion();

	// Server: fired once after any change to an axis' OwnerUPID (claim, release, leave, abort). A
	// subclass that derives state from ownership (the Habitat phase) recomputes it here. Base: empty.
	virtual void OnAxisOwnershipChanged();

	// Broadcasts OnAxesUpdated to local subscribers (screen UI) AND pushes the axes to the AR puppet.
	// Single funnel so the listen host (server-authored changes, no OnRep) and remote clients (OnRep)
	// both keep UI + puppet in sync. Protected so a subclass that mutates Axes (completion) can notify.
	void NotifyAxesUpdated();

	// --- Designer tuning (§10) ---

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minigame")
	float AxisToleranceDeg = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minigame")
	float DwellSeconds = 2.5f;

	// Anti-teleport clamp: the largest angle (degrees) a single input message may add.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minigame")
	float MaxStepPerInputDeg = 45.0f;

	UPROPERTY(ReplicatedUsing = OnRep_UpdateAxes, BlueprintReadOnly, Category = "Minigame")
	TArray<FAxisData> Axes;

	UFUNCTION()
	void OnRep_UpdateAxes();

	static float NormalizeDeg(float Angle);
	static float AngularDistanceDeg(float A, float B);

private:
	// Mechanics (server).
	void ClaimAxis(const FString& UPID, int32 AxisIndex);
	void ReleaseAxis(const FString& UPID, int32 AxisIndex);
	void ReleaseAxesOf(const FString& UPID);
	void RotateAxis(const FString& UPID, int32 AxisIndex, float DeltaDegrees);

	// Empties every axis owned by UPID WITHOUT firing OnAxisOwnershipChanged. Returns whether anything
	// changed. ClaimAxis uses it so a re-claim raises the ownership hook once, not twice.
	bool ClearOwnershipOf(const FString& UPID);

	// Server tick: recomputes every axis' bAligned from Value vs GetAxisTarget within AxisToleranceDeg.
	// Runs in every state (an Idle building still shows whether it is on target). Returns whether any
	// flag flipped, so the caller can notify local subscribers; clients get it through replication.
	bool RefreshAlignmentFlags();

	// Server tick: refreshes InToleranceTime for UI feedback from bAligned. Does NOT complete the game.
	void UpdateAlignment(float DeltaTime);

	// Server tick while rotation is closed: holds every dwell timer at zero.
	void ResetDwell();

	// --- Client-side rotate gesture state (per local player; one per client) ---
	// The joystick "dial" model: delta = change in stick angle since last frame. Reset on release so
	// the next grab doesn't produce a huge jump.
	float LastStickAngleDeg = 0.0f;
	bool bHasLastStickAngle = false;
};
