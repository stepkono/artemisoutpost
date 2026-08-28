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
	if (bHasTarget || Record.Type != EOutpostBuildingType::Habitat)
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
