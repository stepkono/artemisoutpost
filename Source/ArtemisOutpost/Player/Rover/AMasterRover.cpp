// Fill out your copyright notice in the Description page of Project Settings.


#include "AMasterRover.h"
#include "EngineUtils.h"
#include "PuppetRover.h"
#include "ArtemisOutpost/Cesium/GeoRefsManager.h"
#include "Net/UnrealNetwork.h"

AMasterRover::AMasterRover()
{
	bReplicates = true;
	bAlwaysRelevant = true;
}

void AMasterRover::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	// Call the Super
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
 
	// Add properties to replicated for the derived class
	DOREPLIFETIME(AMasterRover, PuppetRover);
}

void AMasterRover::BeginPlay()
{
	const TCHAR* Net = HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT");
	UE_LOG(LogTemp, Warning, TEXT("[Master][%s] BeginPlay: %s | World=%s"),
		Net, *GetName(), *GetActorLocation().ToString());

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[Master][%s] BeginPlay: Failed to get World. Aborting..."), Net);
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
		UE_LOG(LogTemp, Error, TEXT("[Master][%s] BeginPlay: Failed to get GeoRefsManager."), Net);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[Master][%s] BeginPlay: GeoRefsManager OK | VRMoon=%s | ARMoon=%s"),
		Net, *GetNameSafe(GeoRefsManager->GetVRMoon()), *GetNameSafe(GeoRefsManager->GetARMoon()));

	// Initialize the start position to calc the delta vector in next frames
	const FVector WorldPos = this->GetActorLocation();
	StartLocalPosition_UE  = GeoRefsManager->GetVRMoon()->GetTransform().InverseTransformPositionNoScale(WorldPos);
	UE_LOG(LogTemp, Warning, TEXT("[Master][%s] BeginPlay: WorldPos=%s | StartLocalPos_VR=%s"),
		Net, *WorldPos.ToString(), *StartLocalPosition_UE.ToString());
}

FVector AMasterRover::GetLocalPos_UE() const
{
	const FVector WorldPos_UE = this->GetActorLocation();
	const FVector LocalPos_UE = GeoRefsManager->GetVRMoon()->GetTransform().InverseTransformPositionNoScale(WorldPos_UE);
	
	return LocalPos_UE;
}

FQuat AMasterRover::GetAbsoluteOrientation()
{
	return this->GetActorQuat(); 
}

APuppetRover* AMasterRover::GetPuppetRover()
{
	return PuppetRover;
}
