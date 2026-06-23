// Fill out your copyright notice in the Description page of Project Settings.


#include "PawnController.h"
#include "Net/UnrealNetwork.h"

void APawnController::BeginPlay()
{
	Super::BeginPlay();
}

void APawnController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(APawnController, ARPawn);
	DOREPLIFETIME(APawnController, VRPawn);
	DOREPLIFETIME(APawnController, MasterRover);
	DOREPLIFETIME(APawnController, GeoRefsManager); 
}

void APawnController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	SetViewTarget(InPawn);
	
	if (!ARPawn)
	{
		if (APawnAR* AR = Cast<APawnAR>(InPawn))
		{
			ARPawn = AR;
		}
	}
}

void APawnController::InitializePawns(ACharVR* VRPlayer, AMasterRover* RoverPuppet)
{
	const TCHAR* Net = HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT");
	UE_LOG(LogTemp, Warning, TEXT("[Ctrl][%s] InitializePawns: VRPlayer=%s | RoverPuppet(Master)=%s"),
		Net, *GetNameSafe(VRPlayer), *GetNameSafe(RoverPuppet));

	if (!VRPlayer)
	{
		UE_LOG(LogTemp, Error, TEXT("[Ctrl][%s] InitializePawns: VRPlayer is NULL. Aborting."), Net);
		return; 
	}
	if (!RoverPuppet)
	{
		UE_LOG(LogTemp, Error, TEXT("[Ctrl][%s] InitializePawns: RoverPuppet (Master) is NULL. Aborting."), Net);
		return; 
	}
	
	VRPawn = VRPlayer;
	MasterRover = RoverPuppet;
	
	if (!GeoRefsManager)
	{
		UE_LOG(LogTemp, Error, TEXT("PawnController: GeoRefsManager is NULL. Aborting."))
		return;
	}
	
	VRPawn->SetGeoRefsManager(GeoRefsManager);
	MasterRover->SetGeoRefsManager(GeoRefsManager);
}

void APawnController::SpawnVRPlayer()
{
	if (UWorld* World = GetWorld())
	{
		const FActorSpawnParameters SpawnParams;
		VRPawn = World->SpawnActor<ACharVR>(SpawnParams);
		VRPawn->SetActorHiddenInGame(true);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("PawnController: World not found."));
	}
}

void APawnController::SetGeoRefsManager(AGeoRefsManager* InGeoRefsManager)
{
	GeoRefsManager = InGeoRefsManager;
}
