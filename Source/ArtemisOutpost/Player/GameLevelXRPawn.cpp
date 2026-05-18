// Fill out your copyright notice in the Description page of Project Settings.


#include "GameLevelXRPawn.h"


// Sets default values
AGameLevelXRPawn::AGameLevelXRPawn()
{
	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void AGameLevelXRPawn::BeginPlay()
{
	Super::BeginPlay();
	
	if (UGameInstance* DefaultGI = GetWorld()->GetGameInstance())
	{
		GI = Cast<UArtemisGameInstance>(DefaultGI);
		if (!GI)
		{
			UE_LOG(LogTemp, Error, TEXT("[GameLevelXRPawn]: Failed to cast to custom game instance.")); 
			return; 
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[GameLevelXRPawn]: Failed to get current GameInstance."));
		return; 
	}
	
	// If this client has saved anchors in the current session -> share these anchors
	if (IsAuthoritativeClient())
	{
		ShareAnchorsWithServer(GI->RawAnchors); 
	}
}

// Called every frame
void AGameLevelXRPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// Called to bind functionality to input
void AGameLevelXRPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

bool AGameLevelXRPawn::IsAuthoritativeClient() const
{
	return GI->CheckForInitializedSpatialAnchors(); 	
}

void AGameLevelXRPawn::ShareAnchorsWithServer_Implementation(FCustomAnchors RawAnchors)
{
	AArtemisGameState* GS; 
	if (AGameStateBase* DefaultGS = GetWorld()->GetGameState())
	{
		GS = Cast<AArtemisGameState>(DefaultGS);
		if (!GS)
		{
			UE_LOG(LogTemp, Error, TEXT("GameLevelXRPawn: Failed to cast GameState."))
			return; 
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("GameLevelXRPawn: Failed to get GameState."))
		return; 
	}
	
	GS->WriteRawAnchors(RawAnchors);
}
