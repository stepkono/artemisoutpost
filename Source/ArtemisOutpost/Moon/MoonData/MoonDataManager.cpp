// Fill out your copyright notice in the Description page of Project Settings.


#include "MoonDataManager.h"

#include "EngineUtils.h"
#include "ArtemisOutpost/Cesium/GeoRefsManager.h"
#include "ArtemisOutpost/Miscellaneous/ConstantSettings.h"
#include "ArtemisOutpost/Moon/MoonTexturer.h"
#include "ArtemisOutpost/Networking/ClientServerConnection/NetUtils.h"
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

	const bool bClientCtx = ArtemisNet::IsClientContext(GetNetMode());
	UE_LOG(LogTemp, Warning, TEXT("[MoonDataManager] BeginPlay: NetMode=%d, IsClientContext=%d, GeoRefsManager=%s"),
		(int32)GetNetMode(), bClientCtx ? 1 : 0, GeoRefsManager ? TEXT("OK") : TEXT("NULL"));

	// Setup moon the texturing only on clients
	if (bClientCtx)
	{
		if (UWorld* World = GetWorld())
		{
			for (TActorIterator<ACesium3DTileset> It(World); It; ++It)
			{
				if (It->ActorHasTag(FName("AR_TILESET")))
				{
					ARMoonTileSet = *It;
				}
			}
		}
		if (!ARMoonTileSet)
		{
			UE_LOG(LogTemp, Error, TEXT("[MoonDataManager] Failed to get AR_Tileset (no actor tagged 'AR_TILESET'). Aborting."));
			return;
		}
		UE_LOG(LogTemp, Warning, TEXT("[MoonDataManager] Found AR_TILESET: %s"), *ARMoonTileSet->GetName());

		// The tileset owns the texture/material via its MoonTexturer component; we only push data to it.
		MoonTexturer = ARMoonTileSet->FindComponentByClass<UMoonTexturer>();
		UE_LOG(LogTemp, Warning, TEXT("[MoonDataManager] MoonTexturer on AR_TILESET: %s"),
			MoonTexturer ? TEXT("FOUND") : TEXT("NULL"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[MoonDataManager] Not a client context -> fog texturer not resolved."));
	}
}

// Called every frame
void UMoonDataManager::TickComponent(float DeltaTime, ELevelTick TickType,
                                     FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UMoonDataManager::AddNewAreaScan(FAreaScan AreaScan)
{
	// Server-authoritative. The append replicates AreaScans -> clients run OnRep_UpdateForOfWar.
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
	// A new scan arrived from the server -> refresh what the client renders.
	UE_LOG(LogTemp, Log, TEXT("sdkjhsfdkhjsfdkhjsfdkhjlsfdhjksdf"));
	UpdateFogOfWarTexture();
}

void UMoonDataManager::UpdateFogOfWarTexture()
{
	if (!MoonTexturer || !GeoRefsManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MoonDataManager] UpdateFogOfWarTexture skipped: MoonTexturer=%s, GeoRefsManager=%s"),
			MoonTexturer ? TEXT("OK") : TEXT("NULL"), GeoRefsManager ? TEXT("OK") : TEXT("NULL"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[MoonDataManager] UpdateFogOfWarTexture: %d scans"), AreaScans.Num());

	TArray<FLinearColor> ScanPixels;
	ScanPixels.Reserve(AreaScans.Num());

	for (const FAreaScan& AreaScan : AreaScans)
	{
		// Current AR-moon world position of the scan center (reflects the moon's live transform).
		const FVector ScanCenterWorld = GeoRefsManager->ARMoonCoordsToUECoords(AreaScan.GeoPosition);

		// RGB = world position, A = scan radius. The material mirrors IsPositionExploredARMoon:
		// dist(AbsoluteWorldPosition, ScanCenterWorld) - radius, folded with SmoothMin.
		ScanPixels.Add(FLinearColor(
			static_cast<float>(ScanCenterWorld.X),
			static_cast<float>(ScanCenterWorld.Y),
			static_cast<float>(ScanCenterWorld.Z),
			UConstantSettings::ScanningRadius));
	}

	if (ScanPixels.Num() > 0)
	{
		const FVector P(ScanPixels[0].R, ScanPixels[0].G, ScanPixels[0].B);
		UE_LOG(LogTemp, Warning, TEXT("[MoonDataManager] Scan[0] world=%s |pos|=%.0f radius=%.2f"),
			*P.ToString(), P.Size(), ScanPixels[0].A);
	}

	MoonTexturer->ApplyScanData(ScanPixels);
}


