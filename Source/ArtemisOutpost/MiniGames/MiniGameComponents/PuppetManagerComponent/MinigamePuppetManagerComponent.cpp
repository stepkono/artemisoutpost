// Fill out your copyright notice in the Description page of Project Settings.

#include "MinigamePuppetManagerComponent.h"

#include "ArtemisOutpost/MiniGames/MiniGamePuppet/MinigamePuppet.h"
#include "ArtemisOutpost/Moon/Cesium/GeoRefsManager.h"
#include "EngineUtils.h"

UMinigamePuppetManagerComponent::UMinigamePuppetManagerComponent()
{
	// One-shot spawn + push-driven data; no per-tick work.
	PrimaryComponentTick.bCanEverTick = false;
}

void UMinigamePuppetManagerComponent::BeginPlay()
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

void UMinigamePuppetManagerComponent::CreateARPuppet()
{
	if (ARPuppet)
	{
		return; // already created
	}

	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner || !PuppetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("MinigamePuppetManager: following are null: World, owner or puppet class; puppet not spawned."));
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
	const FTransform LocalMaster = Owner->GetActorTransform().GetRelativeTransform(GeoRefsManager->GetVRMoon()->GetActorTransform());
	const FTransform PuppetWorld = LocalMaster * GeoRefsManager->GetARMoon()->GetActorTransform();
	
	const FVector GeoMaster   = GeoRefsManager->UECoordsToVRMoonCoords(Owner->GetActorLocation());
	const FVector WorldPuppet = GeoRefsManager->ARMoonCoordsToUECoords(GeoMaster); 
	
	UE_LOG(LogTemp, Log, TEXT("MinigamePuppetManager: These locations should be nearly equal: %s and %s"), *PuppetWorld.GetLocation().ToString(), *WorldPuppet.ToString());

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Owner;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARPuppet = World->SpawnActor<AMinigamePuppet>(PuppetClass, PuppetWorld, SpawnParams);
	if (!ARPuppet)
	{
		UE_LOG(LogTemp, Error, TEXT("MinigamePuppetManager: Failed to spawn MiniGamePuppet."))
		return;
	}
	
	// Parent to the AR moon so it inherits the moon's (movable) transform automatically.
	ARPuppet->AttachToActor(GeoRefsManager->GetARMoon(), FAttachmentTransformRules(EAttachmentRule::KeepWorld, false));
}

void UMinigamePuppetManagerComponent::PushState(const EMinigameState NewState)
{
	if (ARPuppet)
	{
		ARPuppet->ApplyState(NewState);
	}
}

void UMinigamePuppetManagerComponent::PushData(const FInstancedStruct& Data)
{
	if (ARPuppet)
	{
		ARPuppet->ApplyData(Data);
	}
}

void UMinigamePuppetManagerComponent::PushStartData(const FInstancedStruct& Data)
{
	if (ARPuppet)
	{
		ARPuppet->InitializeStartData(Data);
	}
}
