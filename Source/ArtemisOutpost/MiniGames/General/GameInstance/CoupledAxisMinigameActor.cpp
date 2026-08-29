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

void ACoupledAxisMinigameActor::OnStart()
{
	Super::OnStart();

	Axes.Reset();
	Axes.SetNum(FMath::Max(1, GetAxisCount()));
	for (int32 i = 0; i < Axes.Num(); ++i)
	{
		Axes[i].AxisIndex = i; // so puppets/UI can branch on which axis this is (Earth vs Habitat)
	}

	NotifyAxesUpdated();
}

void ACoupledAxisMinigameActor::OnAbort()
{
	Super::OnAbort();

	Axes.Reset();
	NotifyAxesUpdated();
}

void ACoupledAxisMinigameActor::OnParticipantLeft(const FString& UPID)
{
	ReleaseAxesOf(UPID);
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
		EvaluateCompletion(DeltaTime);
	}
}

void ACoupledAxisMinigameActor::EvaluateCompletion(float DeltaTime)
{
	bool bAllInTolerance = Axes.Num() > 0;

	for (int32 i = 0; i < Axes.Num(); ++i)
	{
		FAxisData& Axis = Axes[i];
		const bool bInTol = AngularDistanceDeg(Axis.Value, GetAxisTarget(i)) <= AxisToleranceDeg;
		Axis.InToleranceTime = bInTol ? Axis.InToleranceTime + DeltaTime : 0.0f;
		bAllInTolerance &= (Axis.InToleranceTime >= DwellSeconds);
	}

	if (bAllInTolerance)
	{
		// OnComplete sets State = Completed; the tick guard then stops further evaluation.
		OnComplete();
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
