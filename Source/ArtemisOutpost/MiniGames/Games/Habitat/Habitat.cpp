// Fill out your copyright notice in the Description page of Project Settings.

#include "Habitat.h"

#include "ArtemisOutpost/MiniGames/MiniGameComponents/PuppetManagerComponent/MinigamePuppetManagerComponent.h"
#include "Net/UnrealNetwork.h"
#include "Dom/JsonObject.h"

AHabitat::AHabitat()
{
	PrimaryActorTick.bCanEverTick = true;
	MiniGameType = EMiniGameType::Habitat;
}

void AHabitat::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHabitat, Phase);
}

void AHabitat::BeginPlay()
{
	// The coupled base builds the Axes array (server) in its BeginPlay, so the tilt goes in after Super.
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	// Random start tilt per axis. Value is 0..360 with 0 = level, so a negative tilt wraps just below
	// 360; GetSignedTiltDeg unwinds it for meshes and UI.
	const float MinMag = FMath::Min(InitialTiltMinDeg, InitialTiltMaxDeg);
	const float MaxMag = FMath::Max(InitialTiltMinDeg, InitialTiltMaxDeg);
	for (FAxisData& Axis : Axes)
	{
		const float Magnitude = FMath::FRandRange(MinMag, MaxMag);
		const float Sign = FMath::RandBool() ? 1.0f : -1.0f;
		Axis.Value = NormalizeDeg(Sign * Magnitude);
	}

	UE_LOG(LogMinigame, Log, TEXT("[Habitat] %s: initial tilt pitch=%.1f roll=%.1f (tolerance=%.1f, dwell=%.1fs)."),
		*GetName(), GetSignedTiltDeg(AxisPitch), GetSignedTiltDeg(AxisRoll), AxisToleranceDeg, DwellSeconds);

	NotifyAxesUpdated();
	RefreshPhase();
}

FInstancedStruct AHabitat::MakeInitialTypeData() const
{
	// A fresh habitat: not yet activated, not yet assigned to a Signal Tower.
	return FInstancedStruct::Make(FHabitatData());
}

// ---- Coupled-axis rules ----

int32 AHabitat::GetAxisCount() const
{
	return 2;
}

float AHabitat::GetAxisTarget(int32 AxisIndex) const
{
	// Levelling: both axes aim for 0 degrees.
	return 0.0f;
}

bool AHabitat::SolvesAxisOnLeave() const
{
	// Completion is simultaneous and time-based (EvaluateCompletion). A player stepping out must not
	// freeze "their" axis as solved, or two sessions could solve the axes one at a time.
	return false;
}

bool AHabitat::IsRotationOpen(FString& OutReason) const
{
	// Derived on the spot rather than read from Phase, so the client-side gesture (which runs on
	// replicated axes before Phase may have arrived) and the server use the identical rule.
	if (GetState() != EMinigameState::Active)
	{
		OutReason = FString::Printf(TEXT("state is %s, not Active"), *UEnum::GetValueAsString(GetState()));
		return false;
	}
	if (!HabitatRules::AreAllAxesOwned(Axes))
	{
		OutReason = FString::Printf(TEXT("waiting for partner (%d/%d axes owned, both players must be on the rotation screen)"),
			GetOwnedAxisCount(), Axes.Num());
		return false;
	}
	return true;
}

void AHabitat::EvaluateCompletion()
{
	// Only reached while Active and IsRotationOpen (Phase == Leveling). Every axis must have dwelled
	// in tolerance for DwellSeconds at the same time. The base zeroes an axis' timer the moment it
	// drifts out and zeroes all of them when the gate closes, so min() over the timers is exactly
	// "both simultaneously".
	if (Axes.Num() == 0 || DwellSeconds <= 0.0f)
	{
		return;
	}

	float MinDwell = TNumericLimits<float>::Max();
	for (const FAxisData& Axis : Axes)
	{
		MinDwell = FMath::Min(MinDwell, Axis.InToleranceTime);
	}

	if (MinDwell < DwellSeconds)
	{
		return;
	}

	for (FAxisData& Axis : Axes)
	{
		Axis.bSolved = true;
	}

	UE_LOG(LogMinigame, Log, TEXT("[Solve] %s: LEVELLED. Both axes held in tolerance for %.1fs together (pitch=%.1f, roll=%.1f, owners: '%s', '%s') -> minigame complete."),
		*GetName(), DwellSeconds, GetSignedTiltDeg(AxisPitch), GetSignedTiltDeg(AxisRoll),
		*Axes[AxisPitch].OwnerUPID, *Axes[AxisRoll].OwnerUPID);

	OnComplete();
	NotifyAxesUpdated();
}

void AHabitat::OnAxisOwnershipChanged()
{
	Super::OnAxisOwnershipChanged();
	RefreshPhase();
}

// ---- Lifecycle ----

void AHabitat::OnStateChangedNative(EMinigameState NewState)
{
	Super::OnStateChangedNative(NewState);

	// Server only: the phase is authoritative here and replicates to everyone else. Covers the flips
	// OnStart/OnAbort cannot: Idle -> Active before OnStart, Active -> Idle after OnAbort, -> Completed.
	if (HasAuthority())
	{
		RefreshPhase();
	}
}

void AHabitat::OnComplete()
{
	Super::OnComplete();

	// State is Completed now; the base's HandleStateChanged already pushed it into the registry, which
	// is what lets a waiting Signal Tower claim this habitat.
	OnHabitatLevelled();
}

void AHabitat::SyncPuppet()
{
	Super::SyncPuppet();
	PushPhaseToPuppet();
}

TSharedRef<FJsonObject> AHabitat::BuildSnapshot() const
{
	TSharedRef<FJsonObject> Obj = Super::BuildSnapshot();
	Obj->SetStringField(TEXT("phase"), UEnum::GetValueAsString(Phase));
	return Obj;
}

// ---- Phase (derived, replicated) ----

void AHabitat::RefreshPhase()
{
	if (!HasAuthority())
	{
		return;
	}

	const EHabitatPhase NewPhase = HabitatRules::DerivePhase(GetState(), Axes);
	if (NewPhase == Phase)
	{
		return;
	}

	const EHabitatPhase OldPhase = Phase;
	Phase = NewPhase;

	FString Owners;
	for (const FAxisData& Axis : Axes)
	{
		Owners += FString::Printf(TEXT("[%d]='%s' "), Axis.AxisIndex, *Axis.OwnerUPID);
	}

	UE_LOG(LogMinigame, Log, TEXT("[Phase] %s: %s -> %s (state=%s, owners: %s). %s"),
		*GetName(), *UEnum::GetValueAsString(OldPhase), *UEnum::GetValueAsString(NewPhase),
		*UEnum::GetValueAsString(GetState()), *Owners,
		NewPhase == EHabitatPhase::Leveling ? TEXT("Rotation OPEN, dwell running.")
										   : TEXT("Rotation CLOSED, dwell held at zero."));

	HandlePhaseChanged(OldPhase);
}

void AHabitat::OnRep_Phase(EHabitatPhase OldPhase)
{
	HandlePhaseChanged(OldPhase);
}

void AHabitat::HandlePhaseChanged(EHabitatPhase OldPhase)
{
	OnPhaseChanged.Broadcast(OldPhase, Phase);
	PushPhaseToPuppet();
}

void AHabitat::PushPhaseToPuppet()
{
	if (!PuppetManager)
	{
		return;
	}

	FHabitatPhaseData Data;
	Data.Phase = Phase;
	PuppetManager->PushData(FInstancedStruct::Make(Data));
}

// ---- Queries ----

float AHabitat::GetSignedTiltDeg(int32 AxisIndex) const
{
	if (!Axes.IsValidIndex(AxisIndex))
	{
		return 0.0f;
	}
	return FMath::UnwindDegrees(Axes[AxisIndex].Value);
}
