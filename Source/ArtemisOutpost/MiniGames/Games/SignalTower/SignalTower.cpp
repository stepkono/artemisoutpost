// Fill out your copyright notice in the Description page of Project Settings.

#include "SignalTower.h"

#include "ArtemisOutpost/Moon/MoonBuildings/MoonBuildingsManager.h"
#include "ArtemisOutpost/MiniGames/MiniGameComponents/PuppetManagerComponent/MinigamePuppetManagerComponent.h"
#include "Net/UnrealNetwork.h"



void ASignalTower::BeginPlay()
{
	Super::BeginPlay();

	// Server-only: pick targets. The base already generated MGID and registered this tower.
	if (!HasAuthority())
	{
		return;
	}

	// Earth is an arbitrary bearing, chosen once.
	EarthTargetDeg = FMath::FRandRange(0.0f, 360.0f);

	TryClaimTargetHabitat();

	// If no habitat is in range yet, wait for one to be built later and claim it then.
	if (!bHasTarget)
	{
		if (UMoonBuildingsManager* Manager = GetWorld() ? GetWorld()->GetSubsystem<UMoonBuildingsManager>() : nullptr)
		{
			Manager->OnMinigameRegistered.AddUObject(this, &ASignalTower::HandleMinigameRegistered);
		}
	}
}

void ASignalTower::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASignalTower, EarthTargetDeg);
	DOREPLIFETIME(ASignalTower, HabitatTargetDeg);
}

void ASignalTower::TryClaimTargetHabitat()
{
	if (bHasTarget)
	{
		return;
	}

	UMoonBuildingsManager* Manager = GetWorld() ? GetWorld()->GetSubsystem<UMoonBuildingsManager>() : nullptr;
	if (!Manager)
	{
		return;
	}

	FGuid HabitatMGID;
	FVector HabitatLocation;
	if (Manager->TryClaimHabitatFor(GetMGID(), GetActorLocation(), SignalRadius, HabitatMGID, HabitatLocation))
	{
		TargetHabitatMGID = HabitatMGID;
		HabitatTargetDeg  = BearingToDeg(HabitatLocation);
		bHasTarget        = true;

		// Server/listen-host: OnRep won't fire locally, so push the (one-time) target directly.
		PushHabitatTargetToPuppet();
	}
}

void ASignalTower::HandleMinigameRegistered(const FMiniGameRecord& Record)
{
	// Only newly built habitats can give an untargeted tower a target.
	if (bHasTarget || Record.Type != EMiniGameType::Habitat)
	{
		return;
	}
	TryClaimTargetHabitat();
}

float ASignalTower::BearingToDeg(const FVector& WorldLocation) const
{
	const FVector Up      = GetActorUpVector();
	const FVector Forward = FVector::VectorPlaneProject(GetActorForwardVector(), Up).GetSafeNormal();
	const FVector Right   = FVector::CrossProduct(Up, Forward).GetSafeNormal();
	const FVector Dir     = FVector::VectorPlaneProject(WorldLocation - GetActorLocation(), Up).GetSafeNormal();

	if (Dir.IsNearlyZero())
	{
		return 0.0f;
	}

	const float Deg = FMath::RadiansToDegrees(FMath::Atan2(FVector::DotProduct(Dir, Right), FVector::DotProduct(Dir, Forward)));
	return Deg < 0.0f ? Deg + 360.0f : Deg;
}

void ASignalTower::PushHabitatTargetToPuppet()
{
	if (!PuppetManager)
	{
		return;
	}

	FAxisTargetData Target;
	Target.AxisIndex = AxisHabitat;
	Target.TargetDeg = HabitatTargetDeg;
	PuppetManager->PushStartData(FInstancedStruct::Make(Target));
}

void ASignalTower::OnRep_HabitatTarget()
{
	// Client: the one-time target arrived -> hand it to the puppet's target ray.
	PushHabitatTargetToPuppet();
}

void ASignalTower::SyncPuppet()
{
	Super::SyncPuppet();

	// Initial sync after the puppet is spawned (covers the case where the target already replicated).
	PushHabitatTargetToPuppet();
}

int32 ASignalTower::GetAxisCount() const
{
	return 2;
}

float ASignalTower::GetAxisTarget(int32 AxisIndex) const
{
	switch (AxisIndex)
	{
	case AxisEarth:   return EarthTargetDeg;
	case AxisHabitat: return HabitatTargetDeg;
	default:          return 0.0f;
	}
}

bool ASignalTower::CanStart(const FString& UPID, FText& OutReason) const
{
	// A Signal Tower can only be started once it points at a habitat.
	if (!bHasTarget)
	{
		OutReason = NSLOCTEXT("SignalTower", "NoHabitatInRange", "No habitat in range to align to.");
		return false;
	}
	return Super::CanStart(UPID, OutReason);
}

void ASignalTower::OnComplete()
{
	Super::OnComplete();

	OnTowerActivated();
}

void ASignalTower::ProcessInput(UInputAction* InputAction, EInputActionType TriggerEvent)
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
