// Fill out your copyright notice in the Description page of Project Settings.

#include "MoonVisualisationLayer.h"

#include "CesiumGeoreference.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "ArtemisOutpost/GameData/ArtemisPlayerState.h"
#include "ArtemisOutpost/Moon/Cesium/GeoRefsManager.h"
#include "ArtemisOutpost/Networking/ClientServerConnection/NetUtils.h"
#include "ArtemisOutpost/Player/PlayerController/PawnController.h"

AMoonVisualisationLayer::AMoonVisualisationLayer()
{
	PrimaryActorTick.bCanEverTick = true;

	// Pure client visual.
	bReplicates = false;
	SetReplicateMovement(false);

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void AMoonVisualisationLayer::BeginPlay()
{
	Super::BeginPlay();

	// The host renders for nobody; keep the layer idle there.
	if (ArtemisNet::IsServerHost(GetNetMode()))
	{
		SetActorTickEnabled(false);
		return;
	}

	for (TActorIterator<AGeoRefsManager> It(GetWorld()); It; ++It)
	{
		GeoRefsManager = *It;
		break;
	}
	if (!GeoRefsManager)
	{
		UE_LOG(LogTemp, Error, TEXT("[MoonLayer] %s: no AGeoRefsManager in the level; beacons cannot be placed."), *GetName());
		return;
	}

	Moon = ResolveMoon(GeoRefsManager);
	if (!Moon)
	{
		UE_LOG(LogTemp, Error, TEXT("[MoonLayer] %s: the georeference this layer belongs to is not set on the GeoRefsManager."), *GetName());
		return;
	}

	if (bAttachToMoon && !bAttached)
	{
		AttachToActor(Moon, FAttachmentTransformRules(EAttachmentRule::KeepWorld, false));
		bAttached = true;
	}

	UE_LOG(LogTemp, Log, TEXT("[MoonLayer] %s ready on moon '%s'."), *GetName(), *Moon->GetName());
}

void AMoonVisualisationLayer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!GeoRefsManager || !Moon)
	{
		return;
	}
	SyncFromPlayerStates();
}

FVector AMoonVisualisationLayer::GeoToWorld(const FVector& LonLatHeight) const
{
	return GeoRefsManager ? GeoToWorldOnMoon(GeoRefsManager, LonLatHeight) : FVector::ZeroVector;
}

TArray<FMoonBeacon> AMoonVisualisationLayer::GetActiveBeacons() const
{
	TArray<FMoonBeacon> Out;
	ActiveBeacons.GenerateValueArray(Out);
	return Out;
}

FString AMoonVisualisationLayer::MakeKey(const FString& UPID, EBeaconSource Source)
{
	return UPID + (Source == EBeaconSource::Pointer ? TEXT("|P") : TEXT("|G"));
}

void AMoonVisualisationLayer::SyncFromPlayerStates()
{
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!GS)
	{
		return;
	}

	const APawnController* LocalPC = Cast<APawnController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
	const FString LocalUPID = LocalPC ? LocalPC->GetPlayerUPID() : FString();

	TSet<FString> Seen;

	for (const APlayerState* PS : GS->PlayerArray)
	{
		const AArtemisPlayerState* APS = Cast<AArtemisPlayerState>(PS);
		if (!APS || APS->GetUPID().IsEmpty())
		{
			continue;
		}
		if (!bShowOwnCues && APS->GetUPID() == LocalUPID)
		{
			continue;
		}

		const FPlayerCueState& State = APS->GetCueState();

		if (bShowPointerBeacons && State.bPointing && State.PointerTarget.IsValid()
			&& ShouldShow(State.Context, EBeaconSource::Pointer))
		{
			FMoonBeacon B;
			B.UPID          = APS->GetUPID();
			B.PlayerNumber  = APS->GetPlayerNumber();
			B.Source        = EBeaconSource::Pointer;
			B.SourceContext = State.Context;
			B.Target        = State.PointerTarget;
			B.WorldLocation = GeoToWorld(State.PointerTarget.GeoHit);
			Upsert(B, Seen);
		}

		if (bShowGazeMarkers && State.GazeTarget.IsValid()
			&& ShouldShow(State.Context, EBeaconSource::Gaze))
		{
			FMoonBeacon B;
			B.UPID          = APS->GetUPID();
			B.PlayerNumber  = APS->GetPlayerNumber();
			B.Source        = EBeaconSource::Gaze;
			B.SourceContext = State.Context;
			B.Target        = State.GazeTarget;
			B.WorldLocation = GeoToWorld(State.GazeTarget.GeoHit);
			Upsert(B, Seen);
		}
	}

	// Everything not seen this tick is gone.
	for (auto It = ActiveBeacons.CreateIterator(); It; ++It)
	{
		if (!Seen.Contains(It.Key()))
		{
			OnBeaconRemoved(It.Value().UPID, It.Value().Source);
			It.RemoveCurrent();
		}
	}
}

void AMoonVisualisationLayer::Upsert(const FMoonBeacon& Beacon, TSet<FString>& SeenKeys)
{
	const FString Key = MakeKey(Beacon.UPID, Beacon.Source);
	SeenKeys.Add(Key);

	if (const FMoonBeacon* Existing = ActiveBeacons.Find(Key))
	{
		const bool bChanged = !Existing->Target.IsSameTarget(Beacon.Target)
			|| !Existing->WorldLocation.Equals(Beacon.WorldLocation, 0.01)
			|| Existing->SourceContext != Beacon.SourceContext;
		if (!bChanged)
		{
			return;
		}
	}

	ActiveBeacons.Add(Key, Beacon);
	OnBeaconUpdated(Beacon);
}
