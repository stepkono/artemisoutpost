// Fill out your copyright notice in the Description page of Project Settings.

#include "SignalTower.h"

#include "ArtemisOutpost/Moon/MoonBuildings/MoonMiniGamesManager.h"
#include "ArtemisOutpost/MiniGames/MiniGameComponents/PuppetManagerComponent/MinigamePuppetManagerComponent.h"
#include "Net/UnrealNetwork.h"

ASignalTower::ASignalTower()
{
	MiniGameType = EMiniGameType::SignalTower;
}

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

	// No claimable habitat yet: either none is in range, or the one in range is not levelled yet. Wait
	// for a habitat to register (built later) or to complete (levelled later) and claim it then.
	if (!bHasTarget)
	{
		if (UMoonMiniGamesManager* Manager = GetWorld() ? GetWorld()->GetSubsystem<UMoonMiniGamesManager>() : nullptr)
		{
			Manager->OnMinigameRegistered.AddUObject(this, &ASignalTower::HandleMinigameRegistered);
			Manager->OnMinigameStateChanged.AddUObject(this, &ASignalTower::HandleMinigameStateChanged);
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

	UMoonMiniGamesManager* Manager = GetWorld() ? GetWorld()->GetSubsystem<UMoonMiniGamesManager>() : nullptr;
	if (!Manager)
	{
		UE_LOG(LogMinigame, Error, TEXT("[Claim] %s: UMoonMiniGamesManager subsystem not found -> cannot look for a habitat."), *GetName());
		return;
	}

	FGuid HabitatMGID;

	// Comes back in UE world space (the manager converts each record's geodetic position for us),
	// which is what BearingToDeg needs since it works against this actor's transform.
	FVector HabitatUELocation;

	UE_LOG(LogMinigame, Log, TEXT("[Claim] %s: searching for a habitat within %.0f of %s (UE world space), MGID=%s."),
		*GetName(), SignalRadius, *GetActorLocation().ToString(),
		*GetMGID().ToString(EGuidFormats::DigitsWithHyphens));

	if (Manager->TryClaimHabitatFor(GetMGID(), GetActorLocation(), SignalRadius, HabitatMGID, HabitatUELocation))
	{
		TargetHabitatMGID = HabitatMGID;
		HabitatTargetDeg  = BearingToDeg(HabitatUELocation);
		bHasTarget        = true;

		UE_LOG(LogMinigame, Log, TEXT("[Claim] %s: CLAIMED habitat %s at %s (UE world) -> HabitatTargetDeg=%.1f. The tower can now be started."),
			*GetName(), *HabitatMGID.ToString(EGuidFormats::DigitsWithHyphens),
			*HabitatUELocation.ToString(), HabitatTargetDeg);

		// Server/listen-host: OnRep won't fire locally, so push the (one-time) target directly.
		PushHabitatTargetToPuppet();
	}
	else
	{
		UE_LOG(LogMinigame, Warning, TEXT("[Claim] %s: NO habitat claimed. The tower stays unstartable until one registers in range."), *GetName());
	}
}

void ASignalTower::HandleMinigameRegistered(UProviderDataBase& ProviderData, const EGameEventType GameEvent)
{
	// Only newly built habitats can give an untargeted tower a target.
	// TODO: how much performance does this casting cost 
	// TODO: might be better to create secondary event extra for the Aggregator to reduce casting on in game events
	const UMiniGameProviderData* MiniGameData = Cast<UMiniGameProviderData>(&ProviderData);
	if (!MiniGameData)
	{
		// Was a log-and-fall-through, which then dereferenced the null pointer below.
		UE_LOG(LogMinigame, Error, TEXT("[Claim] %s: registration payload is not a UMiniGameProviderData -> ignored."), *GetName());
		return;
	}

	if (bHasTarget)
	{
		return;
	}

	if (MiniGameData->Type != EMiniGameType::Habitat)
	{
		UE_LOG(LogMinigame, Verbose, TEXT("[Claim] %s: ignoring registration of type %s, still waiting for a Habitat."),
			*GetName(), *UEnum::GetValueAsString(MiniGameData->Type));
		return;
	}

	UE_LOG(LogMinigame, Log, TEXT("[Claim] %s: a Habitat registered -> retrying the claim."), *GetName());

	TryClaimTargetHabitat();
}

void ASignalTower::HandleMinigameStateChanged(const FGuid& MiniGameID, EMiniGameType Type, EMinigameState NewState)
{
	if (bHasTarget)
	{
		return;
	}

	// Only a habitat that just finished its levelling can turn from unclaimable into claimable.
	if (Type != EMiniGameType::Habitat || NewState != EMinigameState::Completed)
	{
		return;
	}

	UE_LOG(LogMinigame, Log, TEXT("[Claim] %s: habitat %s reached Completed (levelled) -> retrying the claim."),
		*GetName(), *MiniGameID.ToString(EGuidFormats::DigitsWithHyphens));

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

		// THIS is the gate that blocks a tower that looks perfectly placed. bHasTarget is only set
		// by TryClaimTargetHabitat, either at BeginPlay or when a habitat registers later, so read
		// the [Claim] lines above to see which of those two paths failed and why.
		UE_LOG(LogMinigame, Warning, TEXT("[CanStart] %s: REFUSED -> no habitat claimed (bHasTarget=false). SignalRadius=%.0f. See the [Claim] logs for the rejected candidates."),
			*GetName(), SignalRadius);
		return false;
	}

	UE_LOG(LogMinigame, Log, TEXT("[CanStart] %s: OK -> habitat %s claimed, HabitatTargetDeg=%.1f, EarthTargetDeg=%.1f."),
		*GetName(), *TargetHabitatMGID.ToString(EGuidFormats::DigitsWithHyphens), HabitatTargetDeg, EarthTargetDeg);

	return Super::CanStart(UPID, OutReason);
}

void ASignalTower::OnComplete()
{
	Super::OnComplete();

	// The tower is aligned to Earth and to its habitat: that IS the habitat's activation (§8.7).
	// Registry first, so the flag is already set when BP reacts with score / radius / VFX.
	if (UMoonMiniGamesManager* Manager = GetWorld() ? GetWorld()->GetSubsystem<UMoonMiniGamesManager>() : nullptr)
	{
		if (TargetHabitatMGID.IsValid())
		{
			Manager->MarkHabitatActivated(TargetHabitatMGID, GetMGID());
		}
		else
		{
			UE_LOG(LogMinigame, Error, TEXT("[Activate] %s completed without a claimed habitat (TargetHabitatMGID invalid) -> nothing to activate. CanStart should have refused this."),
				*GetName());
		}
	}
	else
	{
		UE_LOG(LogMinigame, Error, TEXT("[Activate] %s: UMoonMiniGamesManager subsystem not found -> habitat not marked activated."), *GetName());
	}

	OnTowerActivated();
}
