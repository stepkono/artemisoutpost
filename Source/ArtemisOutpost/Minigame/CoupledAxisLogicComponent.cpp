// Fill out your copyright notice in the Description page of Project Settings.

#include "ArtemisOutpost/Minigame/CoupledAxisLogicComponent.h"
#include "ArtemisOutpost/Connection/ConnectionComponent.h"
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

void UCoupledAxisLogicComponent::OnStart()
{
	Super::OnStart();

	Axes.Reset();
	Axes.SetNum(FMath::Max(1, GetAxisCount()));
	InitAxisTargets();
	ReassignAxisOwnership();

	SetComponentTickEnabled(true);
	
	OnAlignmentUpdated(Axes);
}

void UCoupledAxisLogicComponent::OnAbort()
{
	Super::OnAbort();
	SetComponentTickEnabled(false);
	
	Axes.Reset();
	
	OnAlignmentUpdated(Axes);
}

void UCoupledAxisLogicComponent::OnParticipantJoined(const FString& UPID)
{
	ReassignAxisOwnership();
}

void UCoupledAxisLogicComponent::OnParticipantLeft(const FString& UPID)
{
	ReassignAxisOwnership();
}

void UCoupledAxisLogicComponent::ApplyInput(const FString& UPID, const FMinigameInput& Input)
{
	if (!Axes.IsValidIndex(Input.AxisIndex) || !CanControlAxis(UPID, Input.AxisIndex))
	{
		return;
	}

	const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;
	const float Step = FMath::Clamp(Input.Delta, -1.0f, 1.0f) * InputSpeedDegPerSec * Dt;

	FAxisState& Axis = Axes[Input.AxisIndex];
	Axis.Value = NormalizeDeg(Axis.Value + Step);

	OnAlignmentUpdated(Axes);
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
	if (!Axes.IsValidIndex(AxisIndex))
	{
		return false;
	}
	const FString& Owner = Axes[AxisIndex].OwnerUPID;
	return Owner.IsEmpty() || Owner == UPID;
}

void UCoupledAxisLogicComponent::ReassignAxisOwnership()
{
	const UConnectionComponent* Connection = GetConnection();
	if (!Connection)
	{
		return;
	}

	const TArray<FString> Participants = Connection->GetParticipantUPIDs();

	if (Participants.Num() <= 1)
	{
		for (FAxisState& Axis : Axes)
		{
			Axis.OwnerUPID.Empty();
		}
		return;
	}

	// Partition axes one-per-participant in join order: the joiner takes the axis the earlier
	// participant does not own (§8.7). Extra participants share the last axis.
	for (int32 i = 0; i < Axes.Num(); ++i)
	{
		Axes[i].OwnerUPID = Participants[FMath::Min(i, Participants.Num() - 1)];
	}
}

float UCoupledAxisLogicComponent::GetAxisProgress(int32 AxisIndex) const
{
	if (!Axes.IsValidIndex(AxisIndex) || DwellSeconds <= 0.0f)
	{
		return 0.0f;
	}
	return FMath::Clamp(Axes[AxisIndex].InToleranceTime / DwellSeconds, 0.0f, 1.0f);
}

void UCoupledAxisLogicComponent::OnRep_UpdateAxes()
{
	OnAlignmentUpdated(Axes);
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
