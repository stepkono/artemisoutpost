// Fill out your copyright notice in the Description page of Project Settings.


#include "MoonResourcesManager.h"

#include "ResourceVeinSpline.h"
#include "ArtemisOutpost/Player/PlayerController/PawnController.h"


void UMoonResourcesManager::RegisterVein(UResourceVeinSpline* Vein)
{
	if (!Vein || Veins.Contains(Vein))
	{
		return;
	}

	Veins.Add(Vein);

	// Aggregate this vein's per-sample hooks into the subsystem-level, vein-tagged seam.
	// AddUObject appends the payload (Vein) after the delegate's own args.
	Vein->OnSamplesDiscovered.AddUObject(this, &UMoonResourcesManager::HandleVeinDiscovered, Vein);
	Vein->OnSamplesMined.AddUObject(this, &UMoonResourcesManager::HandleVeinMined, Vein);
}

void UMoonResourcesManager::UnregisterVein(UResourceVeinSpline* Vein)
{
	if (!Vein)
	{
		return;
	}

	Vein->OnSamplesDiscovered.RemoveAll(this);
	Vein->OnSamplesMined.RemoveAll(this);
	Veins.Remove(Vein);
}

APawnController* UMoonResourcesManager::GetLocalPawnController() const
{
	const UWorld* World = GetWorld();
	return World ? Cast<APawnController>(World->GetFirstPlayerController()) : nullptr;
}

void UMoonResourcesManager::ClientReportScan(const FVector& HitWorld, float RadiusWorld)
{
	APawnController* PC = GetLocalPawnController();
	if (!PC)
	{
		return;
	}

	TArray<int32> NewIndices;
	for (UResourceVeinSpline* Vein : Veins)
	{
		if (!IsValid(Vein))
		{
			continue;
		}
		Vein->DetectNewDiscovered(HitWorld, RadiusWorld, NewIndices);
		if (NewIndices.Num() > 0)
		{
			PC->ServerReportVeinDiscovered(Vein, NewIndices);   // event-gated: only on new flips
		}
	}
}

void UMoonResourcesManager::ClientReportMining(const FVector& HitWorld, float RadiusWorld, float DeltaSeconds, float RatePerSecond)
{
	APawnController* PC = GetLocalPawnController();
	if (!PC)
	{
		return;
	}

	TArray<int32> Completed;
	for (UResourceVeinSpline* Vein : Veins)
	{
		if (!IsValid(Vein))
		{
			continue;
		}
		// Advances the local thinning prediction AND returns samples that just completed.
		Vein->PredictMining(HitWorld, RadiusWorld, DeltaSeconds, RatePerSecond, Completed);
		if (Completed.Num() > 0)
		{
			PC->ServerReportVeinMined(Vein, Completed);
		}
	}
}

void UMoonResourcesManager::ServerReportDiscovered(UResourceVeinSpline* Vein, const TArray<int32>& Indices)
{
	if (IsValid(Vein) && Veins.Contains(Vein))
	{
		Vein->ServerMarkDiscovered(Indices);
	}
}

void UMoonResourcesManager::ServerReportMined(UResourceVeinSpline* Vein, const TArray<int32>& Indices)
{
	if (IsValid(Vein) && Veins.Contains(Vein))
	{
		Vein->ServerMarkMinedOut(Indices);
	}
}

ERessourceType UMoonResourcesManager::GetResourceTypeAt(const FVector& GeoPos, float QueryRadiusWorld) const
{
	for (const UResourceVeinSpline* Vein : Veins)
	{
		if (!IsValid(Vein))
		{
			continue;
		}
		const ERessourceType Type = Vein->QueryResourceAt(GeoPos, QueryRadiusWorld);
		if (Type != ERessourceType::NoneRessource)
		{
			return Type;
		}
	}

	return ERessourceType::NoneRessource;
}

void UMoonResourcesManager::HandleVeinDiscovered(const TArray<FVector>& GeoPositions, UResourceVeinSpline* Vein)
{
	UMoonResourceProviderData* Data = NewObject<UMoonResourceProviderData>(this);
	Data->ResourceVein = Vein;
	Data->GeoPositions = GeoPositions;

	OnVeinDiscovered.Broadcast(*Data, EGameEventType::ResourceDiscovered);
}

void UMoonResourcesManager::HandleVeinMined(const TArray<FVector>& GeoPositions, UResourceVeinSpline* Vein)
{
	UMoonResourceProviderData* Data = NewObject<UMoonResourceProviderData>(this);
	Data->ResourceVein = Vein;
	Data->GeoPositions = GeoPositions;

	OnVeinMined.Broadcast(*Data, EGameEventType::ResourceMined);
}
