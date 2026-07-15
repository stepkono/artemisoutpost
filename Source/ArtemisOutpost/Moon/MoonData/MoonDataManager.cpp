// Fill out your copyright notice in the Description page of Project Settings.


#include "MoonDataManager.h"

#include "EngineUtils.h"
#include "ArtemisOutpost/Cesium/GeoRefsManager.h"
#include "ArtemisOutpost/Miscellaneous/ConstantSettings.h"
#include "Net/UnrealNetwork.h"

namespace
{
	// Polynomiales Smooth-Minimum (Inigo Quilez). K = Verrundungsbreite.
	// Ergebnis ist IMMER <= FMath::Min(A, B) -> die Vereinigung beult an Nähten leicht aus.
	float SmoothMin(float A, float B, float K)
	{
		if (K <= 0.f) { return FMath::Min(A, B); }
		const float H = FMath::Clamp(0.5f + 0.5f * (B - A) / K, 0.f, 1.f);
		return FMath::Lerp(B, A, H) - K * H * (1.f - H);
	}
}

void UMoonDataManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(UMoonDataManager, AreaScans);
}

// Sets default values for this component's properties
UMoonDataManager::UMoonDataManager()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}


// Called when the game starts
void UMoonDataManager::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AGeoRefsManager> It(World); It; ++It)
		{
			GeoRefsManager = *It; 
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("MoonDataManager: Failed to get World."))
	}
	
	if (!GeoRefsManager)
	{
		UE_LOG(LogTemp, Error, TEXT("MoonDataManager: Failed to get GeoRefsManager.")); 
	}
}


// Called every frame
void UMoonDataManager::TickComponent(float DeltaTime, ELevelTick TickType,
                                     FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UMoonDataManager::AddNewAreaScan_Implementation(FAreaScan AreaScan)
{
	AreaScans.Add(AreaScan);
}

bool UMoonDataManager::IsPositionExploredARMoon(FVector& GeoPosition, bool bForARMooon)
{
	if (!GeoRefsManager)
	{
		UE_LOG(LogTemp, Error, TEXT("MoonDataManager: GeoRefsManager was not initialized upon Moon Position query. Returning false (unexplored)."));
		return false; 
	}
	
	FVector QueryPosWorld;
	if (bForARMooon)
	{
		QueryPosWorld = GeoRefsManager->ARMoonCoordsToUECoords(GeoPosition);
	}
	else
	{
		QueryPosWorld = GeoRefsManager->VRMoonCoordsToUECoords(GeoPosition);
	}
	
	float Field = TNumericLimits<float>::Max();
	
	for (const FAreaScan& AreaScan : AreaScans)
	{
		FVector ScanAreaCenterWorld;
		if (bForARMooon)
		{
			ScanAreaCenterWorld = GeoRefsManager->ARMoonCoordsToUECoords(AreaScan.GeoPosition);
		}
		else
		{
			ScanAreaCenterWorld = GeoRefsManager->VRMoonCoordsToUECoords(AreaScan.GeoPosition);
		}
		
		const float DistToCenter = FVector::Dist(QueryPosWorld, ScanAreaCenterWorld);
		const float SignedDist = DistToCenter - UConstantSettings::ScanningRadius; 
		
		Field = SmoothMin(Field, SignedDist, UConstantSettings::SmoothingFactor);
		
		if (Field <= 0.f)
		{
			return true;
		}
	}
	
	return Field <= 0.f;
}

void UMoonDataManager::OnRep_UpdateForOfWar()
{
	
}
