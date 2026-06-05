// Fill out your copyright notice in the Description page of Project Settings.


#include "PuppetRover.h"

#include "AMasterRover.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

void APuppetRover::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	// Call the Super
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
 
	// Add properties to replicated for the derived class
	DOREPLIFETIME(APuppetRover, Master);
}

// Sets default values
APuppetRover::APuppetRover()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
}

// Called when the game starts or when spawned
void APuppetRover::BeginPlay()
{
	Super::BeginPlay();

	const TCHAR* Net = HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT");
	UE_LOG(LogTemp, Warning, TEXT("[Puppet][%s] BeginPlay: %s | Master=%s"),
		Net, *GetName(), *GetNameSafe(Master));

	SetReplicateMovement(false);

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[Puppet][%s] BeginPlay: Failed to get World. Aborting..."), Net);
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
		UE_LOG(LogTemp, Error, TEXT("[Puppet][%s] BeginPlay: Failed to get GeoRefsManager."), Net);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[Puppet][%s] BeginPlay: GeoRefsManager OK | VRMoon=%s | ARMoon=%s"),
		Net, *GetNameSafe(GeoRefsManager->GetVRMoon()), *GetNameSafe(GeoRefsManager->GetARMoon()));

	const FAttachmentTransformRules AttachmentTransformRules(
		EAttachmentRule::KeepWorld,
		EAttachmentRule::KeepWorld,
		EAttachmentRule::KeepWorld,
		true
	);

	// To account for scaling for the local delta vector for position
	GeoRefScalingFactor = GeoRefsManager->GetARMoon()->GetActorScale3D().X / GeoRefsManager->GetVRMoon()->GetActorScale3D().X;
	UE_LOG(LogTemp, Warning, TEXT("[Puppet][%s] BeginPlay: ARScale=%s | VRScale=%s | GeoRefScalingFactor=%f"),
		Net, *GeoRefsManager->GetARMoon()->GetActorScale3D().ToString(),
		*GeoRefsManager->GetVRMoon()->GetActorScale3D().ToString(), GeoRefScalingFactor);

	USceneComponent* ARMoonRoot = GeoRefsManager->GetARMoon()->GetRootComponent();
	this->GetRootComponent()->AttachToComponent(ARMoonRoot, AttachmentTransformRules);

	UE_LOG(LogTemp, Warning, TEXT("[Puppet][%s] BeginPlay: Attached to ARMoon root '%s'. AttachParent=%s"),
		Net, *GetNameSafe(ARMoonRoot), *GetNameSafe(GetRootComponent()->GetAttachParent()));
}

// Called every frame
void APuppetRover::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	// Throttle the per-frame logs (~ every 120 frames) so the output stays readable.
	const bool bLogThisFrame = (GFrameCounter % 120 == 0);
	const TCHAR* Net = HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT");

	if (!Master)
	{
		if (bLogThisFrame)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Puppet][%s] Tick: Master not set yet (waiting on replication)."), Net);
		}
		return;
	}

	if (!GeoRefsManager)
	{
		if (bLogThisFrame)
		{
			UE_LOG(LogTemp, Error, TEXT("[Puppet][%s] Tick: GeoRefsManager is NULL."), Net);
		}
		return;
	}

	// Position
	const FVector MasterWorldPos = Master->GetActorLocation();
	const FVector MasterLocalPos_VRMoon_UE = GeoRefsManager->GetVRMoon()->GetTransform().InverseTransformPosition(MasterWorldPos);
	//const FVector MasterLocalPos_ARMoon_UE = MasterLocalPos_VRMoon_UE * GeoRefScalingFactor;
	this->SetActorRelativeLocation(MasterLocalPos_VRMoon_UE);

	// Orientation
	const FQuat MasterOrientation_World = Master->GetAbsoluteOrientation();
	const FQuat MasterOrientation_Local = GeoRefsManager->GetVRMoon()->GetActorQuat().Inverse() * MasterOrientation_World;
	this->SetActorRelativeRotation(MasterOrientation_Local);

	if (bLogThisFrame)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Puppet][%s] Tick: MasterWorld=%s | RelLoc(VRlocal)=%s | PuppetWorld=%s"),
			Net, *MasterWorldPos.ToString(), *MasterLocalPos_VRMoon_UE.ToString(), *GetActorLocation().ToString());
	}
}

void APuppetRover::SetMaster(AMasterRover* MasterRover)
{
	const TCHAR* Net = HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT");
	UE_LOG(LogTemp, Warning, TEXT("[Puppet][%s] SetMaster: %s -> Master=%s"),
		Net, *GetName(), *GetNameSafe(MasterRover));

	Master = MasterRover;
}
