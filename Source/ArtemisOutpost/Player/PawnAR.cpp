// Fill out your copyright notice in the Description page of Project Settings.


#include "PawnAR.h"
#include "Cesium3DTileset.h"
#include "EngineUtils.h"


// Sets default values
APawnAR::APawnAR()
{
	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void APawnAR::BeginPlay()
{
	Super::BeginPlay();
	
	UE_LOG(LogTemp, Display, TEXT("PawnAR: BeginPlay()"));
	
	if (UGameInstance* DefaultGI = GetWorld()->GetGameInstance())
	{
		GI = Cast<UArtemisGameInstance>(DefaultGI);
		if (!GI)
		{
			UE_LOG(LogTemp, Error, TEXT("[PawnAR]: Failed to cast to custom game instance.")); 
			return; 
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[PawnAR]: Failed to get current GameInstance."));
		return; 
	}
	
	const UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[PawnAR]: Failed to cast to custom game instance.")); 
		return; 
	}
	for (auto TileSet : TActorRange<ACesium3DTileset>(World))
	{
		if (TileSet->ActorHasTag(FName("AR_TILESET")))
		{
			ARTileSet = TileSet;
			ARTileSet->SetActorHiddenInGame(false);
			UE_LOG(LogTemp, Error, TEXT("[PawnAR]: TileSet is shown.")); 
		}
		else
		{
			TileSet->SetActorHiddenInGame(true);
		}
	}
	
	if (IsLocallyControlled())
	{
		// If this client has saved anchors in the current session -> share these anchors
		if (IsAuthoritativeClient())
		{
			UE_LOG(LogTemp, Display, TEXT("PawnAR: Calling anchor sharing from authoritative client."))
			ShareAnchorsWithServer(GI->RawAnchors); 
		}	
	}
}

// Called every frame
void APawnAR::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// Called to bind functionality to input
void APawnAR::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

bool APawnAR::IsAuthoritativeClient() const
{
	return GI->CheckForInitializedSpatialAnchors(); 	
}

void APawnAR::ShareAnchorsWithServer_Implementation(FOrderedAnchors RawAnchors)
{
	AArtemisGameState* GS; 
	if (AGameStateBase* DefaultGS = GetWorld()->GetGameState())
	{
		GS = Cast<AArtemisGameState>(DefaultGS);
		if (!GS)
		{
			UE_LOG(LogTemp, Error, TEXT("PawnAR: Failed to cast GameState."))
			return; 
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("PawnAR: Failed to get GameState."))
		return; 
	}
	
	UE_LOG(LogTemp, Display, TEXT("Server: PawnAR: Writing shared anchors to game state..."));
	GS->WriteRawAnchors(RawAnchors);
}

void APawnAR::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();
	
	// Check locally 
	if (GetLocalRole() == ROLE_AutonomousProxy)
	{
		AController* CurrentController = GetController();
		
		if (!ARTileSet)
		{
			UE_LOG(LogTemp, Warning, TEXT("PawnAR: Tileset it not set. Might be initial set, then the warning can be ignored."))
			return; 
		}
		
		// UNPOSSESSED
		if (CurrentController == nullptr)
		{
			ARTileSet->SetActorHiddenInGame(true);
		}
		//POSSESSED
		else
		{
			ARTileSet->SetActorHiddenInGame(false);
		}		
	}
}
