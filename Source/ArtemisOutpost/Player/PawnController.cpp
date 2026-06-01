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

	UE_LOG(LogTemp, Warning, TEXT("PawnController: === OnPossess ==="));
	UE_LOG(LogTemp, Warning, TEXT("InPawn: %s"), InPawn ? *InPawn->GetName() : TEXT("NULL"));
	UE_LOG(LogTemp, Warning, TEXT("ViewTarget after set: %s"), GetViewTarget() ? *GetViewTarget()->GetName() : TEXT("NULL"));
}

void APawnController::InitializePawns()
{
	SpawnVRCharacter(); 
	SpawnARCharacter(); 
}

void APawnController::SpawnARCharacter()
{
	// 
}

void APawnController::SpawnVRCharacter()
{
	if (UWorld* World = GetWorld())
	{
		const FActorSpawnParameters SpawnParams;
		VRPawn = World->SpawnActor<ACharVR>(SpawnParams);
		VRPawn->GetRootComponent()->SetVisibility(false); // TODO: not sure if this will work as expected
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("PawnController: World not found."));
	}
}
