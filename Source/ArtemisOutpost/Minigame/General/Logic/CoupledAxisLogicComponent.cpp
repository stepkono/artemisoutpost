// Fill out your copyright notice in the Description page of Project Settings.

#include "CoupledAxisLogicComponent.h"

#include "Net/UnrealNetwork.h"
#include "Dom/JsonObject.h"

UCoupledAxisLogicComponent::UCoupledAxisLogicComponent()
{
	// Server ticks to run dwell/completion evaluation while Active.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UCoupledAxisLogicComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UCoupledAxisLogicComponent, Axes);
}

int32 UCoupledAxisLogicComponent::GetAxisCount() const
{
	return 2;
}

void UCoupledAxisLogicComponent::InitAxisTargets()
{
}

const TArray<FAxisState>& UCoupledAxisLogicComponent::GetAxes() const
{
	return Axes;
}

int32 UCoupledAxisLogicComponent::GetNumAxes() const
{
	return Axes.Num();
}

float UCoupledAxisLogicComponent::GetDwellSeconds() const
{
	return DwellSeconds;
}

void UCoupledAxisLogicComponent::OnStart()
{
	Super::OnStart();

	Axes.Reset();
	Axes.SetNum(FMath::Max(1, GetAxisCount()));
	InitAxisTargets();

	SetComponentTickEnabled(true);
	OnAxesUpdated.Broadcast(Axes);
}

void UCoupledAxisLogicComponent::OnAbort()
{
	Super::OnAbort();
	SetComponentTickEnabled(false);

	Axes.Reset();
	OnAxesUpdated.Broadcast(Axes);
}

void UCoupledAxisLogicComponent::OnParticipantJoined(const FString& UPID)
{
	// Nothing to claim yet — the participant picks an axis via the selection screen.
}

void UCoupledAxisLogicComponent::OnParticipantLeft(const FString& UPID)
{
	ReleaseAxesOf(UPID);
	OnAxesUpdated.Broadcast(Axes);
}

void UCoupledAxisLogicComponent::ApplyInput(const FString& UPID, const FMinigameInput& Input)
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

	OnAxesUpdated.Broadcast(Axes);
}

void UCoupledAxisLogicComponent::ClaimAxis(const FString& UPID, int32 AxisIndex)
{
	// Only a still-free axis can be claimed; a participant owns at most one at a time.
	if (!Axes.IsValidIndex(AxisIndex) || !Axes[AxisIndex].OwnerUPID.IsEmpty())
	{
		return;
	}
	ReleaseAxesOf(UPID);
	Axes[AxisIndex].OwnerUPID = UPID;
}

void UCoupledAxisLogicComponent::ReleaseAxis(const FString& UPID, int32 AxisIndex)
{
	if (Axes.IsValidIndex(AxisIndex) && Axes[AxisIndex].OwnerUPID == UPID)
	{
		Axes[AxisIndex].OwnerUPID.Empty();
	}
}

void UCoupledAxisLogicComponent::ReleaseAxesOf(const FString& UPID)
{
	for (FAxisState& Axis : Axes)
	{
		if (Axis.OwnerUPID == UPID)
		{
			Axis.OwnerUPID.Empty();
		}
	}
}

void UCoupledAxisLogicComponent::RotateAxis(const FString& UPID, int32 AxisIndex, float DeltaDegrees)
{
	if (!CanControlAxis(UPID, AxisIndex))
	{
		return;
	}

	const float Step = FMath::Clamp(DeltaDegrees, -MaxStepPerInputDeg, MaxStepPerInputDeg);
	FAxisState& Axis = Axes[AxisIndex];
	Axis.Value = NormalizeDeg(Axis.Value + Step);
}

void UCoupledAxisLogicComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (GetOwner() && GetOwner()->HasAuthority() && GetState() == EMinigameState::Active)
	{
		EvaluateCompletion(DeltaTime);
	}
}

void UCoupledAxisLogicComponent::EvaluateCompletion(float DeltaTime)
{
	bool bAllInTolerance = Axes.Num() > 0;

	for (FAxisState& Axis : Axes)
	{
		const bool bInTol = AngularDistanceDeg(Axis.Value, Axis.TargetValue) <= AxisToleranceDeg;
		Axis.InToleranceTime = bInTol ? Axis.InToleranceTime + DeltaTime : 0.0f;
		bAllInTolerance &= (Axis.InToleranceTime >= DwellSeconds);
	}

	if (bAllInTolerance)
	{
		SetComponentTickEnabled(false);
		OnComplete();
	}
}

bool UCoupledAxisLogicComponent::CanControlAxis(const FString& UPID, int32 AxisIndex) const
{
	return Axes.IsValidIndex(AxisIndex) && Axes[AxisIndex].OwnerUPID == UPID;
}

float UCoupledAxisLogicComponent::GetAxisProgress(int32 AxisIndex) const
{
	if (!Axes.IsValidIndex(AxisIndex) || DwellSeconds <= 0.0f)
	{
		return 0.0f;
	}
	return FMath::Clamp(Axes[AxisIndex].InToleranceTime / DwellSeconds, 0.0f, 1.0f);
}

bool UCoupledAxisLogicComponent::IsAxisAligned(int32 AxisIndex) const
{
	if (!Axes.IsValidIndex(AxisIndex))
	{
		return false;
	}
	return AngularDistanceDeg(Axes[AxisIndex].Value, Axes[AxisIndex].TargetValue) <= AxisToleranceDeg;
}

void UCoupledAxisLogicComponent::OnRep_UpdateAxes()
{
	OnAxesUpdated.Broadcast(Axes);
}

TSharedRef<FJsonObject> UCoupledAxisLogicComponent::BuildSnapshot() const
{
	TSharedRef<FJsonObject> Obj = Super::BuildSnapshot();

	TArray<TSharedPtr<FJsonValue>> AxisArray;
	for (const FAxisState& Axis : Axes)
	{
		TSharedRef<FJsonObject> A = MakeShared<FJsonObject>();
		A->SetNumberField(TEXT("value"), Axis.Value);
		A->SetNumberField(TEXT("target"), Axis.TargetValue);
		A->SetStringField(TEXT("owner"), Axis.OwnerUPID);
		AxisArray.Add(MakeShared<FJsonValueObject>(A));
	}
	Obj->SetArrayField(TEXT("axes"), AxisArray);
	return Obj;
}

float UCoupledAxisLogicComponent::NormalizeDeg(float Angle)
{
	Angle = FMath::Fmod(Angle, 360.0f);
	return Angle < 0.0f ? Angle + 360.0f : Angle;
}

float UCoupledAxisLogicComponent::AngularDistanceDeg(float A, float B)
{
	const float Diff = FMath::Abs(NormalizeDeg(A) - NormalizeDeg(B));
	return FMath::Min(Diff, 360.0f - Diff);
}
