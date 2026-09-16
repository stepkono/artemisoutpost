// Fill out your copyright notice in the Description page of Project Settings.

#include "MoonMiniGamesManager.h"

#include "ArtemisOutpost/Moon/Cesium/GeoRefsManager.h"
#include "EngineUtils.h"

AGeoRefsManager* UMoonMiniGamesManager::GetGeoRefsManager()
{
	if (CachedGeoRefsManager)
	{
		return CachedGeoRefsManager;
	}

	if (const UWorld* World = GetWorld())
	{
		for (TActorIterator<AGeoRefsManager> It(World); It; ++It)
		{
			CachedGeoRefsManager = *It;
		}
	}

	return CachedGeoRefsManager;
}

void UMoonMiniGamesManager::RegisterMinigame(const FMiniGameRecord& Record)
{
	if (!Record.MGID.IsValid())
	{
		UE_LOG(LogMinigame, Warning, TEXT("[Register] RegisterMinigame with invalid MGID; ignored. This building will never be findable."));
		return;
	}

	Buildings.Add(Record.MGID, Record);

	UE_LOG(LogMinigame, Log, TEXT("[Register] registry now holds %d building(s); added %s (%s) at %s."),
		Buildings.Num(), *Record.MGID.ToString(EGuidFormats::DigitsWithHyphens),
		*UEnum::GetValueAsString(Record.Type), *Record.BuildGeoLocation.ToString());

	UMiniGameProviderData* NewData = NewObject<UMiniGameProviderData>();
	NewData->MGID = Record.MGID;
	NewData->BuildLocation = Record.BuildGeoLocation;
	NewData->MiniGameData = Record.MiniGameData;
	NewData->State = Record.State;
	// Was missing: without it Type stayed at its default (SignalTower) for every registration, so
	// listeners filtering on Type (e.g. a tower waiting for a Habitat) never matched.
	NewData->Type = Record.Type;
	
	OnMinigameRegistered.Broadcast(*NewData, EGameEventType::NewMiniGamePlaced);
}

void UMoonMiniGamesManager::UpdateState(const FGuid& MGID, EMinigameState NewState)
{
	FMiniGameRecord* Record = Buildings.Find(MGID);
	if (!Record)
	{
		UE_LOG(LogMinigame, Warning, TEXT("[Register] UpdateState(%s -> %s): MGID is not in the registry -> ignored."),
			*MGID.ToString(EGuidFormats::DigitsWithHyphens), *UEnum::GetValueAsString(NewState));
		return;
	}

	if (Record->State == NewState)
	{
		return;
	}

	Record->State = NewState;
	OnMinigameStateChanged.Broadcast(MGID, Record->Type, NewState);
}

bool UMoonMiniGamesManager::MarkHabitatActivated(const FGuid& HabitatMGID, const FGuid& ByTowerMGID)
{
	FMiniGameRecord* Record = Buildings.Find(HabitatMGID);
	if (!Record)
	{
		UE_LOG(LogMinigame, Error, TEXT("[Activate] tower %s: habitat %s is not in the registry -> nothing activated."),
			*ByTowerMGID.ToString(EGuidFormats::DigitsWithHyphens), *HabitatMGID.ToString(EGuidFormats::DigitsWithHyphens));
		return false;
	}
	if (Record->Type != EMiniGameType::Habitat)
	{
		UE_LOG(LogMinigame, Error, TEXT("[Activate] tower %s: %s is a %s, not a Habitat -> nothing activated."),
			*ByTowerMGID.ToString(EGuidFormats::DigitsWithHyphens), *HabitatMGID.ToString(EGuidFormats::DigitsWithHyphens),
			*UEnum::GetValueAsString(Record->Type));
		return false;
	}

	FHabitatData* Habitat = Record->MiniGameData.GetMutablePtr<FHabitatData>();
	if (!Habitat)
	{
		UE_LOG(LogMinigame, Error, TEXT("[Activate] tower %s: habitat %s carries no FHabitatData -> nothing activated."),
			*ByTowerMGID.ToString(EGuidFormats::DigitsWithHyphens), *HabitatMGID.ToString(EGuidFormats::DigitsWithHyphens));
		return false;
	}

	if (Habitat->bActivated)
	{
		UE_LOG(LogMinigame, Warning, TEXT("[Activate] tower %s: habitat %s was already activated -> no change."),
			*ByTowerMGID.ToString(EGuidFormats::DigitsWithHyphens), *HabitatMGID.ToString(EGuidFormats::DigitsWithHyphens));
		return false;
	}

	Habitat->bActivated = true;
	UE_LOG(LogMinigame, Log, TEXT("[Activate] habitat %s ACTIVATED by tower %s."),
		*HabitatMGID.ToString(EGuidFormats::DigitsWithHyphens), *ByTowerMGID.ToString(EGuidFormats::DigitsWithHyphens));
	return true;
}

bool UMoonMiniGamesManager::TryClaimHabitatFor(const FGuid& TowerMGID, const FVector& TowerUELocation, float Radius,
	FGuid& OutHabitatMGID, FVector& OutHabitatUELocation)
{
	// Records hold geodetic positions, the caller asks in UE world space, so the georeference is
	// mandatory here. Without it every candidate would be compared in the wrong space and the tower
	// would silently never find a habitat.
	AGeoRefsManager* GeoRefs = GetGeoRefsManager();
	if (!GeoRefs)
	{
		UE_LOG(LogMinigame, Error, TEXT("[Claim] TryClaimHabitatFor: no AGeoRefsManager in the level -> cannot convert habitat geo positions to UE space. Aborting the search."));
		return false;
	}

	const float RadiusSq = Radius * Radius;

	FGuid BestMGID;
	FVector BestUELocation = FVector::ZeroVector;
	float BestDistSq = TNumericLimits<float>::Max();
	bool bFound = false;

	int32 HabitatCount = 0;

	UE_LOG(LogMinigame, Log, TEXT("[Claim] TryClaimHabitatFor: %d building(s) registered, radius=%.0f, tower at %s (UE world)."),
		Buildings.Num(), Radius, *TowerUELocation.ToString());

	for (const TPair<FGuid, FMiniGameRecord>& Pair : Buildings)
	{
		const FMiniGameRecord& Record = Pair.Value;
		if (Record.Type != EMiniGameType::Habitat)
		{
			UE_LOG(LogMinigame, Verbose, TEXT("[Claim]   skip %s: type is %s, not Habitat."),
				*Pair.Key.ToString(EGuidFormats::DigitsWithHyphens), *UEnum::GetValueAsString(Record.Type));
			continue;
		}
		++HabitatCount;

		const FHabitatData* Habitat = Record.MiniGameData.GetPtr<FHabitatData>();
		if (!Habitat)
		{
			// The habitat actor must override MakeInitialTypeData() to attach an FHabitatData.
			UE_LOG(LogMinigame, Warning, TEXT("[Claim]   skip %s: Habitat record carries NO FHabitatData payload."),
				*Pair.Key.ToString(EGuidFormats::DigitsWithHyphens));
			continue;
		}
		if (Record.State != EMinigameState::Completed)
		{
			// The foundation has not been levelled yet (Habitat minigame not completed). A tower may only
			// point at a finished habitat; it retries when this record reaches Completed.
			UE_LOG(LogMinigame, Warning, TEXT("[Claim]   skip %s: habitat is not levelled yet (state=%s, needs Completed)."),
				*Pair.Key.ToString(EGuidFormats::DigitsWithHyphens), *UEnum::GetValueAsString(Record.State));
			continue;
		}
		if (Habitat->bActivated)
		{
			UE_LOG(LogMinigame, Warning, TEXT("[Claim]   skip %s: habitat is already activated."),
				*Pair.Key.ToString(EGuidFormats::DigitsWithHyphens));
			continue;
		}
		if (Habitat->bAssignedToSignalTower)
		{
			// Assignment is permanent by design, so a habitat serves exactly one tower ever.
			UE_LOG(LogMinigame, Warning, TEXT("[Claim]   skip %s: habitat is permanently assigned to tower %s."),
				*Pair.Key.ToString(EGuidFormats::DigitsWithHyphens),
				*Habitat->AssignedTowerMGID.ToString(EGuidFormats::DigitsWithHyphens));
			continue;
		}
		
		// geo (lon/lat/height) -> UE world, so the range test happens entirely in UE space.
		const FVector HabitatUELocation = GeoRefs->VRMoonCoordsToUECoords(Record.BuildGeoLocation);

		const float DistSq = FVector::DistSquared(TowerUELocation, HabitatUELocation);
		if (DistSq > RadiusSq)
		{
			UE_LOG(LogMinigame, Warning, TEXT("[Claim]   skip %s: out of range. dist=%.0f > radius=%.0f. Habitat geo=%s -> UE=%s, tower UE=%s."),
				*Pair.Key.ToString(EGuidFormats::DigitsWithHyphens),
				FMath::Sqrt(DistSq), Radius,
				*Record.BuildGeoLocation.ToString(), *HabitatUELocation.ToString(), *TowerUELocation.ToString());
			continue;
		}

		// Nearest wins; on a tie the first encountered stays (order-dependent, acceptable per design).
		if (!bFound || DistSq < BestDistSq)
		{
			bFound = true;
			BestDistSq = DistSq;
			BestMGID = Pair.Key;
			BestUELocation = HabitatUELocation;
		}
	}

	if (!bFound)
	{
		UE_LOG(LogMinigame, Warning, TEXT("[Claim] TryClaimHabitatFor: FAILED. %d habitat(s) in the registry, none eligible (see the skip reasons above)."),
			HabitatCount);
		return false;
	}

	UE_LOG(LogMinigame, Log, TEXT("[Claim] TryClaimHabitatFor: picked %s at dist=%.0f for tower %s."),
		*BestMGID.ToString(EGuidFormats::DigitsWithHyphens), FMath::Sqrt(BestDistSq),
		*TowerMGID.ToString(EGuidFormats::DigitsWithHyphens));

	// Commit the claim atomically (synchronous, single game thread → no race).
	FMiniGameRecord& Winner = Buildings[BestMGID];
	if (FHabitatData* Habitat = Winner.MiniGameData.GetMutablePtr<FHabitatData>())
	{
		Habitat->bAssignedToSignalTower = true;
		Habitat->AssignedTowerMGID = TowerMGID;
	}

	OutHabitatMGID = BestMGID;

	// UE world space, matching TowerUELocation: the caller needs it to compute a bearing against its
	// own actor transform. The record keeps the geodetic position untouched.
	OutHabitatUELocation = BestUELocation;
	return true;
}
