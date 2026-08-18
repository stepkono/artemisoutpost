// Fill out your copyright notice in the Description page of Project Settings.

#include "MinigamePuppetManager.h"

#include "ArtemisOutpost/Minigame/General/GameInstance/MinigamePuppet.h"
#include "ArtemisOutpost/Cesium/GeoRefsManager.h"
#include "EngineUtils.h"

UMinigamePuppetManager::UMinigamePuppetManager()
{
	// One-shot spawn + push-driven data; no per-tick work.
	PrimaryComponentTick.bCanEverTick = false;
}

void UMinigamePuppetManager::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<AGeoRefsManager> It(World); It; ++It)
	{
		GeoRefsManager = *It;
		break;
	}
}

void UMinigamePuppetManager::CreateARPuppet()
{
	if (ARPuppet)
	{
		return; // already created
	}

	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner || !PuppetClass)
	{
		return;
	}
	if (!GeoRefsManager || !GeoRefsManager->GetVRMoon() || !GeoRefsManager->GetARMoon())
	{
		UE_LOG(LogTemp, Warning, TEXT("MinigamePuppetManager: missing GeoRefsManager/moon; puppet not spawned."));
		return;
	}

	// The master's pose relative to the VR moon, re-anchored on the AR moon. UE convention:
	// World = Relative * Parent, so Relative = Master.GetRelativeTransform(VRMoon), then * ARMoon.
	// This propagates scale correctly: the puppet scales with the AR moon.
	const FTransform LocalMaster = Owner->GetActorTransform()
		.GetRelativeTransform(GeoRefsManager->GetVRMoon()->GetActorTransform());
	const FTransform PuppetWorld = LocalMaster * GeoRefsManager->GetARMoon()->GetActorTransform();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Owner;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARPuppet = World->SpawnActor<AMinigamePuppet>(PuppetClass, PuppetWorld, SpawnParams);
	if (!ARPuppet)
	{
		return;
	}

	// Parent to the AR moon so it inherits the moon's (movable) transform automatically.
	ARPuppet->AttachToActor(GeoRefsManager->GetARMoon(),
		FAttachmentTransformRules(EAttachmentRule::KeepWorld, false));
}

void UMinigamePuppetManager::PushState(EMinigameState NewState)
{
	if (ARPuppet)
	{
		ARPuppet->ApplyState(NewState);
	}
}

void UMinigamePuppetManager::PushAxes(const TArray<FAxisState>& Axes)
{
	if (ARPuppet)
	{
		ARPuppet->ApplyAxes(Axes);
	}
}
