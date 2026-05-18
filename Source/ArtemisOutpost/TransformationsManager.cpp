// Fill out your copyright notice in the Description page of Project Settings.

#include "TransformationsManager.h"

#include "OculusXRAnchorBPFunctionLibrary.h"
#include "GameData/ArtemisGameState.h"

#pragma region Constructors
void UTransformationsManager::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	
	AArtemisGameState* GS; 
	if (AGameStateBase* DefaultGS = GetWorld()->GetGameState())
	{
		GS = Cast<AArtemisGameState>(DefaultGS);
		if (!GS)
		{
			UE_LOG(LogTemp, Error, TEXT("TransformationsManager: Failed to cast to custom game state."));
			return; 
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("TransformationsManager: Failed to get the default game state."));
		return;
	}
	
	GS->OnMapCoordinatesReceived.AddDynamic(this, &UTransformationsManager::OnNewBaseCoordinates);
}

void UTransformationsManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UTransformationsManager::Deinitialize()
{
	if (const UWorld* World = GetWorld())
	{
		
	}
	
	Super::Deinitialize();
}

TStatId UTransformationsManager::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTransformationsManager, STATGROUP_Tickables);
}

void UTransformationsManager::Tick(float DeltaTime)
{
	// UTickableWorldSubsystem ticks via FTickableGameObject — no Super::Tick
}
#pragma endregion 

void UTransformationsManager::OnNewBaseCoordinates(FMapBaseCoordinates& BaseCoordinate)
{
	
}

