// Fill out your copyright notice in the Description page of Project Settings.

#include "MoonMiniGamesManager.h"

void UMoonMiniGamesManager::RegisterMinigame(const FMiniGameRecord& Record)
{
	if (!Record.MGID.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("MoonBuildingsManager: RegisterMinigame with invalid MGID; ignored."));
		return;
	}

	Buildings.Add(Record.MGID, Record);
	
	UMiniGameProviderData* NewData = NewObject<UMiniGameProviderData>();
	NewData->MGID = Record.MGID;
	NewData->BuildLocation = Record.BuildLocation;
	NewData->MiniGameData = Record.MiniGameData;
	NewData->State = Record.State;
	
	OnMinigameRegistered.Broadcast(*NewData, EGameEventType::NewMiniGamePlaced);
}

void UMoonMiniGamesManager::UpdateState(const FGuid& MGID, EMinigameState NewState)
{
	if (FMiniGameRecord* Record = Buildings.Find(MGID))
	{
		Record->State = NewState;
	}
}

bool UMoonMiniGamesManager::TryClaimHabitatFor(const FGuid& TowerMGID, const FVector& TowerLocation, float Radius,
	FGuid& OutHabitatMGID, FVector& OutHabitatLocation)
{
	const float RadiusSq = Radius * Radius;

	FGuid BestMGID;
	float BestDistSq = TNumericLimits<float>::Max();
	bool bFound = false;

	for (const TPair<FGuid, FMiniGameRecord>& Pair : Buildings)
	{
		const FMiniGameRecord& Record = Pair.Value;
		if (Record.Type != EMiniGameType::Habitat)
		{
			continue;
		}

		const FHabitatData* Habitat = Record.MiniGameData.GetPtr<FHabitatData>();
		if (!Habitat || Habitat->bActivated || Habitat->bAssignedToSignalTower)
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(TowerLocation, Record.BuildLocation);
		if (DistSq > RadiusSq)
		{
			continue;
		}

		// Nearest wins; on a tie the first encountered stays (order-dependent, acceptable per design).
		if (!bFound || DistSq < BestDistSq)
		{
			bFound = true;
			BestDistSq = DistSq;
			BestMGID = Pair.Key;
		}
	}

	if (!bFound)
	{
		return false;
	}

	// Commit the claim atomically (synchronous, single game thread → no race).
	FMiniGameRecord& Winner = Buildings[BestMGID];
	if (FHabitatData* Habitat = Winner.MiniGameData.GetMutablePtr<FHabitatData>())
	{
		Habitat->bAssignedToSignalTower = true;
		Habitat->AssignedTowerMGID = TowerMGID;
	}

	OutHabitatMGID = BestMGID;
	OutHabitatLocation = Winner.BuildLocation;
	return true;
}
