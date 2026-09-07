// Fill out your copyright notice in the Description page of Project Settings.

#include "CoupledAxisMinigameActor.h"

#include "ArtemisOutpost/MiniGames/Games/SignalTower/SignalTower.h"
#include "ArtemisOutpost/MiniGames/MiniGameComponents/PuppetManagerComponent/MinigamePuppetManagerComponent.h"
#include "Net/UnrealNetwork.h"
#include "Dom/JsonObject.h"

ASignalTower::ASignalTower()
{
	MiniGameType = EMiniGameType::SignalTower; 
}

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

// ---- Lifecycle mechanics ----

void ACoupledAxisMinigameActor::BeginPlay()
{
	Super::BeginPlay();

	// The axes belong to the TOWER, not to a play session: a solved axis and a half-turned axis both
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

	// "Abort" now only means "nobody is playing any more". It used to Axes.Reset(), which would
	// throw away every solved axis the moment the last player pressed the trigger.
	for (FAxisData& Axis : Axes)
	{
		Axis.OwnerUPID.Empty();
		Axis.InToleranceTime = 0.0f;
	}

	NotifyAxesUpdated();
}

void ACoupledAxisMinigameActor::OnParticipantLeft(const FString& UPID)
{
	// Leaving IS the commit. Judge the axis this player was holding by its last input state, mark it
	// solved if it ended up aligned, then hand it back. Evaluated per leave, so a solo player can
	// solve one axis, walk away, come back and solve the other.
	const int32 Axis = GetAxisOwnedBy(UPID);
	if (Axes.IsValidIndex(Axis) && !Axes[Axis].bSolved && IsAxisAligned(Axis))
	{
		Axes[Axis].bSolved = true;

		UE_LOG(LogMinigame, Log, TEXT("[Solve] %s: axis %d left ALIGNED by '%s' (value=%.1f, target=%.1f) -> solved."),
			*GetName(), Axis, *UPID, Axes[Axis].Value, GetAxisTarget(Axis));
	}
	else if (Axes.IsValidIndex(Axis))
	{
		UE_LOG(LogMinigame, Log, TEXT("[Solve] %s: axis %d left MISALIGNED by '%s' (value=%.1f, target=%.1f, tolerance=%.1f) -> stays open at that angle."),
			*GetName(), Axis, *UPID, Axes[Axis].Value, GetAxisTarget(Axis), AxisToleranceDeg);
	}

	ReleaseAxesOf(UPID);

	// Whole task done? Do this BEFORE the base class decides to abort, so that its
	// "State != Completed" guard already sees the finished state.
	if (AreAllAxesSolved() && GetState() != EMinigameState::Completed)
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
	// Only a still-free axis can be claimed; a participant owns at most one at a time.
	if (!Axes.IsValidIndex(AxisIndex) || !Axes[AxisIndex].OwnerUPID.IsEmpty())
	{
		return;
	}

	// A solved axis is finished for good. The UI greys it out, this is the authoritative backstop.
	if (Axes[AxisIndex].bSolved)
	{
		UE_LOG(LogMinigame, Warning, TEXT("[Solve] %s: '%s' tried to claim axis %d, which is already solved."),
			*GetName(), *UPID, AxisIndex);
		return;
	}

	ReleaseAxesOf(UPID);
	Axes[AxisIndex].OwnerUPID = UPID;
}

void ACoupledAxisMinigameActor::ReleaseAxis(const FString& UPID, int32 AxisIndex)
{
	if (Axes.IsValidIndex(AxisIndex) && Axes[AxisIndex].OwnerUPID == UPID)
	{
		Axes[AxisIndex].OwnerUPID.Empty();
	}
}

void ACoupledAxisMinigameActor::ReleaseAxesOf(const FString& UPID)
{
	for (FAxisData& Axis : Axes)
	{
		if (Axis.OwnerUPID == UPID)
		{
			Axis.OwnerUPID.Empty();
		}
	}
}

void ACoupledAxisMinigameActor::RotateAxis(const FString& UPID, int32 AxisIndex, float DeltaDegrees)
{
	if (!CanControlAxis(UPID, AxisIndex))
	{
		return;
	}

	const float Step = FMath::Clamp(DeltaDegrees, -MaxStepPerInputDeg, MaxStepPerInputDeg);
	FAxisData& Axis = Axes[AxisIndex];
	Axis.Value = NormalizeDeg(Axis.Value + Step);
}

// ---- Completion (server tick) ----

void ACoupledAxisMinigameActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (HasAuthority() && GetState() == EMinigameState::Active)
	{
		UpdateAlignment(DeltaTime);
	}
}

// Per-tick BOOKKEEPING only. It keeps InToleranceTime current so the UI can show "you are on
// target / holding steady", and nothing else. It deliberately does NOT finish the game: the task
// is no longer completed by holding still long enough. Whether the tower was solved is decided
// exactly once, when the last participant leaves.
void ACoupledAxisMinigameActor::UpdateAlignment(float DeltaTime)
{
	for (int32 i = 0; i < Axes.Num(); ++i)
	{
		FAxisData& Axis = Axes[i];
		const bool bInTol = AngularDistanceDeg(Axis.Value, GetAxisTarget(i)) <= AxisToleranceDeg;
		Axis.InToleranceTime = bInTol ? Axis.InToleranceTime + DeltaTime : 0.0f;
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
