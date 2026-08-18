// Fill out your copyright notice in the Description page of Project Settings.


#include "PuppetRover.h"
#include "AMasterRover.h"
#include "EngineUtils.h"

// Sets default values
APuppetRover::APuppetRover()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void APuppetRover::BeginPlay()
{
	Super::BeginPlay();

	const UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[Puppet] BeginPlay: Failed to get World. Aborting..."));
		return;
	}

	for (TActorIterator<AGeoRefsManager> It(World); It; ++It)
	{
		if (AGeoRefsManager* Manager = *It)
		{
			GeoRefsManager = Manager;
			break;
		}
	}

	if (!GeoRefsManager)
	{
		UE_LOG(LogTemp, Error, TEXT("[Puppet] BeginPlay: Failed to get GeoRefsManager."));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[Puppet]BeginPlay: GeoRefsManager OK | VRMoon=%s | ARMoon=%s"), *GetNameSafe(GeoRefsManager->GetVRMoon()), *GetNameSafe(GeoRefsManager->GetARMoon()));

	const FAttachmentTransformRules AttachmentTransformRules(
		EAttachmentRule::KeepWorld,
		EAttachmentRule::KeepWorld,
		EAttachmentRule::KeepWorld,
		true
	);

	// To account for scaling for the local delta vector for position
	GeoRefScalingFactor = GeoRefsManager->GetARMoon()->GetActorScale3D().X / GeoRefsManager->GetVRMoon()->GetActorScale3D().X;
	UE_LOG(LogTemp, Warning, TEXT("[Puppet] BeginPlay: ARScale=%s | VRScale=%s | GeoRefScalingFactor=%f"), *GeoRefsManager->GetARMoon()->GetActorScale3D().ToString(),
		*GeoRefsManager->GetVRMoon()->GetActorScale3D().ToString(), GeoRefScalingFactor);

	USceneComponent* ARMoonRoot = GeoRefsManager->GetARMoon()->GetRootComponent();
	this->GetRootComponent()->AttachToComponent(ARMoonRoot, AttachmentTransformRules);

	UE_LOG(LogTemp, Warning, TEXT("[Puppet] BeginPlay: Attached to ARMoon root '%s'. AttachParent=%s"), *GetNameSafe(ARMoonRoot), *GetNameSafe(GetRootComponent()->GetAttachParent()));
}

// Called every frame
void APuppetRover::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void APuppetRover::SetMaster(AMasterRover* MasterRover)
{
	UE_LOG(LogTemp, Warning, TEXT("[Puppet]SetMaster: %s -> Master=%s"), *GetName(), *GetNameSafe(MasterRover));

	Master = MasterRover;
}
