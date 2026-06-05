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
}

void APawnController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	SetViewTarget(InPawn);
	
	if (bInitialPosses)
	{
		bInitialPosses = false;
		
		APawnAR* PlayerPawn = Cast<APawnAR>(InPawn);
		if (!PlayerPawn)
		{
			UE_LOG(LogTemp, Error, TEXT("PlayerController: Failed to cast player pawn to AR Pawn."));
			return; 
		}
		ARPawn = PlayerPawn; 	
	}

	const TCHAR* Net = HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT");
	UE_LOG(LogTemp, Warning, TEXT("[Ctrl][%s] === OnPossess === InPawn=%s | bInitialPosses(after)=%d | ViewTarget=%s"),
		Net, *GetNameSafe(InPawn), bInitialPosses ? 1 : 0, *GetNameSafe(GetViewTarget()));
}

void APawnController::InitializePawns(ACharVR* VRPlayer, AMasterRover* RoverPuppet)
{
	const TCHAR* Net = HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT");
	UE_LOG(LogTemp, Warning, TEXT("[Ctrl][%s] InitializePawns: VRPlayer=%s | RoverPuppet(Master)=%s"),
		Net, *GetNameSafe(VRPlayer), *GetNameSafe(RoverPuppet));

	VRPawn = VRPlayer;
	MasterRover = RoverPuppet;

	if (!VRPlayer)
	{
		UE_LOG(LogTemp, Error, TEXT("[Ctrl][%s] InitializePawns: VRPlayer is NULL."), Net);
	}
	if (!RoverPuppet)
	{
		UE_LOG(LogTemp, Error, TEXT("[Ctrl][%s] InitializePawns: RoverPuppet (Master) is NULL."), Net);
	}
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
