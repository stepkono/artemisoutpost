// Fill out your copyright notice in the Description page of Project Settings.

#include "CoupledAxisMinigameActor.h"

#include "ArtemisOutpost/MiniGames/MiniGameComponents/PuppetManagerComponent/MinigamePuppetManagerComponent.h"
#include "Net/UnrealNetwork.h"
#include "Dom/JsonObject.h"

ACoupledAxisMinigameActor::ACoupledAxisMinigameActor()
{
	// The base already enables actor tick for the connection prompt; we reuse it (server-side) to
	// run the dwell/completion evaluation while Active.
}

void ACoupledAxisMinigameActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACoupledAxisMinigameActor, Axes);
}

const TArray<FAxisData>& ACoupledAxisMinigameActor::GetAxes() const
{
	return Axes;
}

int32 ACoupledAxisMinigameActor::GetNumAxes() const
{
	return Axes.Num();
}

// ---- Coupled-axis rules (base defaults) ----

int32 ACoupledAxisMinigameActor::GetAxisCount() const
{
	return 2;
}

float ACoupledAxisMinigameActor::GetAxisTarget(int32 AxisIndex) const
{
	return 0.0f;
}

bool ACoupledAxisMinigameActor::IsRotationOpen(FString& OutReason) const
{
	return true;
}

bool ACoupledAxisMinigameActor::SolvesAxisOnLeave() const
{
	return true;
}

void ACoupledAxisMinigameActor::EvaluateCompletion()
{
}

void ACoupledAxisMinigameActor::OnAxisOwnershipChanged()
{
}

// ---- Lifecycle mechanics ----

void ACoupledAxisMinigameActor::BeginPlay()
{
	Super::BeginPlay();

	// The axes belong to the BUILDING, not to a play session: a solved axis and a half-turned axis both
	// have to survive everybody leaving and somebody else walking up later. So they are built once
	// here rather than in OnStart, which runs on every fresh session.
	if (HasAuthority())
	{
		Axes.Reset();
		Axes.SetNum(FMath::Max(1, GetAxisCount()));
		for (int32 i = 0; i < Axes.Num(); ++i)
		{
			Axes[i].AxisIndex = i; // so puppets/UI can branch on which axis this is (Earth vs Habitat)
		}

		NotifyAxesUpdated();
	}
}

void ACoupledAxisMinigameActor::OnStart()
{
	Super::OnStart();

	// A new session must NOT touch Value or bSolved. Only the dwell timers are session-scoped.
	for (FAxisData& Axis : Axes)
	{
		Axis.InToleranceTime = 0.0f;
	}

	NotifyAxesUpdated();
}

void ACoupledAxisMinigameActor::OnAbort()
{
	Super::OnAbort();

	// "Abort" only means "nobody is playing any more". Values and bSolved are kept: throwing them away
	// would discard every solved axis the moment the last player pressed the trigger.
	bool bOwnershipChanged = false;
	for (FAxisData& Axis : Axes)
	{
		bOwnershipChanged |= !Axis.OwnerUPID.IsEmpty();
		Axis.OwnerUPID.Empty();
		Axis.InToleranceTime = 0.0f;
	}

	if (bOwnershipChanged)
	{
		OnAxisOwnershipChanged();
	}

	NotifyAxesUpdated();
}

void ACoupledAxisMinigameActor::OnParticipantLeft(const FString& UPID)
{
	const int32 Axis = GetAxisOwnedBy(UPID);

	if (Axes.IsValidIndex(Axis))
	{
		if (!SolvesAxisOnLeave())
		{
			// This game completes from the tick, so stepping out commits nothing. The axis keeps its
			// angle and is handed back below.
			UE_LOG(LogMinigame, Log, TEXT("[Solve] %s: axis %d released by '%s' on leave (value=%.1f). Leaving is not the commit for this game -> stays open at that angle."),
				*GetName(), Axis, *UPID, Axes[Axis].Value);
		}
		else if (!Axes[Axis].bSolved && IsAxisAligned(Axis))
		{
			// Leaving IS the commit. Judge the axis this player was holding by its last input state, mark
			// it solved if it ended up aligned. Evaluated per leave, so a solo player can solve one axis,
			// walk away, come back and solve the other.
			Axes[Axis].bSolved = true;

			UE_LOG(LogMinigame, Log, TEXT("[Solve] %s: axis %d left ALIGNED by '%s' (value=%.1f, target=%.1f) -> solved."),
				*GetName(), Axis, *UPID, Axes[Axis].Value, GetAxisTarget(Axis));
		}
		else
		{
			UE_LOG(LogMinigame, Log, TEXT("[Solve] %s: axis %d left MISALIGNED by '%s' (value=%.1f, target=%.1f, tolerance=%.1f) -> stays open at that angle."),
				*GetName(), Axis, *UPID, Axes[Axis].Value, GetAxisTarget(Axis), AxisToleranceDeg);
		}
	}

	ReleaseAxesOf(UPID);

	// Whole task done? Do this BEFORE the base class decides to abort, so that its
	// "State != Completed" guard already sees the finished state.
	if (SolvesAxisOnLeave() && AreAllAxesSolved() && GetState() != EMinigameState::Completed)
	{
		UE_LOG(LogMinigame, Log, TEXT("[Solve] %s: ALL axes solved -> minigame complete, it can no longer be played."), *GetName());
		OnComplete();
	}

	NotifyAxesUpdated();
}

void ACoupledAxisMinigameActor::ApplyInput(const FString& UPID, const FMinigameInput& Input)
{
	switch (Input.Type)
	{
	case EMinigameInputType::ClaimAxis:
		ClaimAxis(UPID, Input.AxisIndex);
		break;
	case EMinigameInputType::ReleaseAxis:
		ReleaseAxis(UPID, Input.AxisIndex);
		break;
	case EMinigameInputType::Rotate:
		RotateAxis(UPID, Input.AxisIndex, Input.Delta);
		break;
	}

	NotifyAxesUpdated();
}

// ---- Ownership + rotation (server) ----

void ACoupledAxisMinigameActor::ClaimAxis(const FString& UPID, int32 AxisIndex)
{
	if (!Axes.IsValidIndex(AxisIndex))
	{
		UE_LOG(LogMinigame, Warning, TEXT("[Claim] %s: '%s' tried to claim axis %d, which does not exist (%d axes)."),
			*GetName(), *UPID, AxisIndex, Axes.Num());
		return;
	}

	// Only a still-free axis can be claimed; a participant owns at most one at a time.
	if (!Axes[AxisIndex].OwnerUPID.IsEmpty())
	{
		UE_LOG(LogMinigame, Warning, TEXT("[Claim] %s: '%s' tried to claim axis %d, which is held by '%s'. The UI greys it out, this is the authoritative backstop (lost claim race)."),
			*GetName(), *UPID, AxisIndex, *Axes[AxisIndex].OwnerUPID);
		return;
	}

	// A solved axis is finished for good. The UI greys it out, this is the authoritative backstop.
	if (Axes[AxisIndex].bSolved)
	{
		UE_LOG(LogMinigame, Warning, TEXT("[Solve] %s: '%s' tried to claim axis %d, which is already solved."),
			*GetName(), *UPID, AxisIndex);
		return;
	}

	// Swap, not add: whatever this player held before is dropped in the same step, and the ownership
	// hook fires exactly once for the combined change.
	ClearOwnershipOf(UPID);
	Axes[AxisIndex].OwnerUPID = UPID;

	UE_LOG(LogMinigame, Log, TEXT("[Claim] %s: '%s' now owns axis %d (%d/%d axes owned)."),
		*GetName(), *UPID, AxisIndex, GetOwnedAxisCount(), Axes.Num());

	OnAxisOwnershipChanged();
}

void ACoupledAxisMinigameActor::ReleaseAxis(const FString& UPID, int32 AxisIndex)
{
	if (!Axes.IsValidIndex(AxisIndex) || Axes[AxisIndex].OwnerUPID != UPID)
	{
		UE_LOG(LogMinigame, Warning, TEXT("[Claim] %s: '%s' tried to release axis %d, which it does not own (owner='%s')."),
			*GetName(), *UPID, AxisIndex,
			Axes.IsValidIndex(AxisIndex) ? *Axes[AxisIndex].OwnerUPID : TEXT("<invalid index>"));
		return;
	}

	Axes[AxisIndex].OwnerUPID.Empty();
	UE_LOG(LogMinigame, Log, TEXT("[Claim] %s: '%s' released axis %d."), *GetName(), *UPID, AxisIndex);

	OnAxisOwnershipChanged();
}

bool ACoupledAxisMinigameActor::ClearOwnershipOf(const FString& UPID)
{
	bool bChanged = false;
	for (FAxisData& Axis : Axes)
	{
		if (Axis.OwnerUPID == UPID)
		{
			Axis.OwnerUPID.Empty();
			bChanged = true;
		}
	}
	return bChanged;
}

void ACoupledAxisMinigameActor::ReleaseAxesOf(const FString& UPID)
{
	if (ClearOwnershipOf(UPID))
	{
		OnAxisOwnershipChanged();
	}
}

void ACoupledAxisMinigameActor::RotateAxis(const FString& UPID, int32 AxisIndex, float DeltaDegrees)
{
	// Rotate arrives at frame rate, so the refusals below log at Verbose to stay readable. Turn on
	//   Log LogMinigame Verbose
	// to see each dropped step; the phase / claim lines at Log level explain WHY the gate is closed.
	if (!CanControlAxis(UPID, AxisIndex))
	{
		UE_LOG(LogMinigame, Verbose, TEXT("[Rotate] %s: DROPPED %.2f deg from '%s' on axis %d -> not the owner (owner='%s')."),
			*GetName(), DeltaDegrees, *UPID, AxisIndex,
			Axes.IsValidIndex(AxisIndex) ? *Axes[AxisIndex].OwnerUPID : TEXT("<invalid index>"));
		return;
	}

	FString ClosedReason;
	if (!IsRotationOpen(ClosedReason))
	{
		UE_LOG(LogMinigame, Verbose, TEXT("[Rotate] %s: DROPPED %.2f deg from '%s' on axis %d -> rotation closed: %s"),
			*GetName(), DeltaDegrees, *UPID, AxisIndex, *ClosedReason);
		return;
	}

	const float Step = FMath::Clamp(DeltaDegrees, -MaxStepPerInputDeg, MaxStepPerInputDeg);
	FAxisData& Axis = Axes[AxisIndex];
	Axis.Value = NormalizeDeg(Axis.Value + Step);
}

// ---- Dwell + completion (server tick) ----

void ACoupledAxisMinigameActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!HasAuthority() || GetState() != EMinigameState::Active)
	{
		return;
	}

	FString ClosedReason;
	if (IsRotationOpen(ClosedReason))
	{
		UpdateAlignment(DeltaTime);
		EvaluateCompletion();
	}
	else
	{
		// Closed: nobody can move anything, so nobody can "hold steady" either. Keeping the timers at
		// zero means a subclass' dwell-based completion can only be earned while the gate is open.
		ResetDwell();
	}
}

// Per-tick BOOKKEEPING only. It keeps InToleranceTime current so the UI can show "you are on
// target / holding steady". It deliberately does NOT finish the game: that decision is a per-game
// policy (SolvesAxisOnLeave / EvaluateCompletion).
void ACoupledAxisMinigameActor::UpdateAlignment(float DeltaTime)
{
	for (int32 i = 0; i < Axes.Num(); ++i)
	{
		FAxisData& Axis = Axes[i];
		const bool bInTol = AngularDistanceDeg(Axis.Value, GetAxisTarget(i)) <= AxisToleranceDeg;
		Axis.InToleranceTime = bInTol ? Axis.InToleranceTime + DeltaTime : 0.0f;
	}
}

void ACoupledAxisMinigameActor::ResetDwell()
{
	for (FAxisData& Axis : Axes)
	{
		Axis.InToleranceTime = 0.0f;
	}
}

// ---- Client-side input interpretation ----

void ACoupledAxisMinigameActor::ProcessInput(UInputAction* InputAction, EInputActionType TriggerEvent)
{
	// Data-driven dispatch: the BP child maps each concrete InputAction asset to an intent type.
	const EMinigameInputType* Intent = InputActionMap.Find(InputAction);
	if (!Intent)
	{
		return;
	}

	const FString LocalUPID = GetLocalPlayerUPID();

	switch (*Intent)
	{
	case EMinigameInputType::Rotate:
	{
		// Rotate the axis THIS player owns (resolved from ownership, so the input layer needs no
		// axis knowledge). No owned axis -> nothing to turn.
		const int32 Axis = GetAxisOwnedBy(LocalUPID);
		if (Axis == INDEX_NONE)
		{
			return;
		}

		// Gesture end (stick released): reset so the next grab starts fresh, no delta jump.
		if (TriggerEvent == EInputActionType::Completed || TriggerEvent == EInputActionType::Canceled)
		{
			bHasLastStickAngle = false;
			return;
		}
		if (TriggerEvent != EInputActionType::Triggered)
		{
			return;
		}

		// Read the current stick from the local player's Enhanced Input. Deadzone is already applied
		// by the action's EnhancedInput modifier -> a centered stick reads ~zero.
		const FVector2D Stick = GetLocalActionValue(InputAction);
		if (Stick.IsNearlyZero())
		{
			bHasLastStickAngle = false;
			return;
		}

		// Dial model: delta = change in the stick's angle since last frame (circle the stick to turn).
		const float CurrentAngle = FMath::RadiansToDegrees(FMath::Atan2(Stick.Y, Stick.X));
		if (!bHasLastStickAngle)
		{
			LastStickAngleDeg = CurrentAngle;
			bHasLastStickAngle = true;
			return; // first frame of the gesture: set the reference, emit no delta yet
		}

		const float Delta = FMath::FindDeltaAngleDegrees(LastStickAngleDeg, CurrentAngle);
		LastStickAngleDeg = CurrentAngle;

		// Gate closed (e.g. the Habitat is waiting for the second player): the reference angle above
		// keeps following the stick so nothing accumulates, but no delta is sent. The server refuses
		// anyway; this only saves the RPCs. Derived from the replicated axes, so it may lag the server
		// by a frame, which is harmless in both directions.
		FString ClosedReason;
		if (!IsRotationOpen(ClosedReason))
		{
			return;
		}

		FMinigameInput In;
		In.Type = EMinigameInputType::Rotate;
		In.AxisIndex = Axis;
		In.Delta = Delta;
		SubmitInput(In);
		break;
	}

	case EMinigameInputType::ReleaseAxis:
	{
		if (TriggerEvent != EInputActionType::Started)
		{
			return;
		}
		const int32 Axis = GetAxisOwnedBy(LocalUPID);
		if (Axis == INDEX_NONE)
		{
			return;
		}
		FMinigameInput In;
		In.Type = EMinigameInputType::ReleaseAxis;
		In.AxisIndex = Axis;
		SubmitInput(In);
		break;
	}

	case EMinigameInputType::ClaimAxis:
		// Claim needs an explicit TARGET axis (which one to grab) -> that comes from the axis-selection
		// screen (widget), which calls SubmitInput with the chosen index. A generic input action can't
		// carry "which axis", so it is not handled here.
		break;
	}
}

// ---- Queries ----

bool ACoupledAxisMinigameActor::CanControlAxis(const FString& UPID, int32 AxisIndex) const
{
	return Axes.IsValidIndex(AxisIndex) && Axes[AxisIndex].OwnerUPID == UPID;
}

int32 ACoupledAxisMinigameActor::GetAxisOwnedBy(const FString& UPID) const
{
	for (int32 i = 0; i < Axes.Num(); ++i)
	{
		if (Axes[i].OwnerUPID == UPID)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

int32 ACoupledAxisMinigameActor::GetOwnedAxisCount() const
{
	int32 Count = 0;
	for (const FAxisData& Axis : Axes)
	{
		if (!Axis.OwnerUPID.IsEmpty())
		{
			++Count;
		}
	}
	return Count;
}

float ACoupledAxisMinigameActor::GetAxisProgress(int32 AxisIndex) const
{
	if (!Axes.IsValidIndex(AxisIndex) || DwellSeconds <= 0.0f)
	{
		return 0.0f;
	}
	return FMath::Clamp(Axes[AxisIndex].InToleranceTime / DwellSeconds, 0.0f, 1.0f);
}

bool ACoupledAxisMinigameActor::IsAxisAligned(int32 AxisIndex) const
{
	if (!Axes.IsValidIndex(AxisIndex))
	{
		return false;
	}
	return AngularDistanceDeg(Axes[AxisIndex].Value, GetAxisTarget(AxisIndex)) <= AxisToleranceDeg;
}

bool ACoupledAxisMinigameActor::IsAxisSolved(int32 AxisIndex) const
{
	return Axes.IsValidIndex(AxisIndex) && Axes[AxisIndex].bSolved;
}

bool ACoupledAxisMinigameActor::AreAllAxesSolved() const
{
	if (Axes.Num() == 0)
	{
		return false;
	}

	for (const FAxisData& Axis : Axes)
	{
		if (!Axis.bSolved)
		{
			return false;
		}
	}
	return true;
}

void ACoupledAxisMinigameActor::OnRep_UpdateAxes()
{
	NotifyAxesUpdated();
}

void ACoupledAxisMinigameActor::NotifyAxesUpdated()
{
	OnAxesUpdated.Broadcast(Axes);

	if (PuppetManager)
	{
		for (const FAxisData& Axis : Axes)
		{
			PuppetManager->PushData(FInstancedStruct::Make(Axis));
		}
	}
}

void ACoupledAxisMinigameActor::SyncPuppet()
{
	Super::SyncPuppet();

	if (PuppetManager)
	{
		for (const FAxisData& Axis : Axes)
		{
			PuppetManager->PushData(FInstancedStruct::Make(Axis));
		}
	}
}

TSharedRef<FJsonObject> ACoupledAxisMinigameActor::BuildSnapshot() const
{
	TSharedRef<FJsonObject> Obj = Super::BuildSnapshot();

	TArray<TSharedPtr<FJsonValue>> AxisArray;
	for (int32 i = 0; i < Axes.Num(); ++i)
	{
		const FAxisData& Axis = Axes[i];
		TSharedRef<FJsonObject> A = MakeShared<FJsonObject>();
		A->SetNumberField(TEXT("value"), Axis.Value);
		A->SetNumberField(TEXT("target"), GetAxisTarget(i));
		A->SetStringField(TEXT("owner"), Axis.OwnerUPID);
		A->SetBoolField(TEXT("solved"), Axis.bSolved);
		AxisArray.Add(MakeShared<FJsonValueObject>(A));
	}
	Obj->SetArrayField(TEXT("axes"), AxisArray);
	return Obj;
}

float ACoupledAxisMinigameActor::NormalizeDeg(float Angle)
{
	Angle = FMath::Fmod(Angle, 360.0f);
	return Angle < 0.0f ? Angle + 360.0f : Angle;
}

float ACoupledAxisMinigameActor::AngularDistanceDeg(float A, float B)
{
	const float Diff = FMath::Abs(NormalizeDeg(A) - NormalizeDeg(B));
	return FMath::Min(Diff, 360.0f - Diff);
}
