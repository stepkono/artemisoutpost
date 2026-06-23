// Fill out your copyright notice in the Description page of Project Settings.


#include "AMasterRover.h"
#include "Cesium3DTileset.h"
#include "EngineUtils.h"
#include "PuppetRover.h"
#include "ArtemisOutpost/Cesium/GeoRefsManager.h"

AMasterRover::AMasterRover()
{
	bReplicates = true;
	bAlwaysRelevant = true;
}

void AMasterRover::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	// Call the Super
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
}

void AMasterRover::BeginPlay()
{
	const TCHAR* Net = HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT");
	UE_LOG(LogTemp, Warning, TEXT("[Master][%s] BeginPlay: %s | World=%s"),
		Net, *GetName(), *GetActorLocation().ToString());

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[Master][%s] BeginPlay: Failed to get World. Aborting..."), Net);
		return;
	}

	for (TActorIterator<AGeoRefsManager> It(World); It; ++It)
	{
		if (AGeoRefsManager* Manager = *It)
		{
			GeoRefsManager = Manager;
			break;
		}
	}

	if (!GeoRefsManager)
	{
		UE_LOG(LogTemp, Error, TEXT("[Master][%s] BeginPlay: Failed to get GeoRefsManager."), Net);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[Master][%s] BeginPlay: GeoRefsManager OK | VRMoon=%s | ARMoon=%s"),
		Net, *GetNameSafe(GeoRefsManager->GetVRMoon()), *GetNameSafe(GeoRefsManager->GetARMoon()));

	// Initialize the start position to calc the delta vector in next frames
	const FVector WorldPos = this->GetActorLocation();
	StartLocalPosition_UE  = GeoRefsManager->GetVRMoon()->GetTransform().InverseTransformPositionNoScale(WorldPos);
	UE_LOG(LogTemp, Warning, TEXT("[Master][%s] BeginPlay: WorldPos=%s | StartLocalPos_VR=%s"),
		Net, *WorldPos.ToString(), *StartLocalPosition_UE.ToString());
		
	Super::BeginPlay();
}

void AMasterRover::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	// Throttle the per-frame logs to roughly once every LogIntervalSeconds.
	LogTimeAccumulator += DeltaTime;
	bool bLogThisFrame = false;
	if (LogTimeAccumulator >= LogIntervalSeconds)
	{
		LogTimeAccumulator -= LogIntervalSeconds;
		bLogThisFrame = true;
	}
	const TCHAR* Net = HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT");

	// ---- Server-side collision probe ----
	// Physics for the master rover is authoritative on the server, so the only collision
	// that matters for "falling through" is the server's. Trace straight down (toward the
	// VR moon centre) and report whether a Cesium physics mesh is actually present beneath
	// the rover. A MISS means tiles aren't streamed on the server, the tileset has no
	// physics meshes, or collision is disabled — any of which causes the fall-through.
	if (HasAuthority() && bLogThisFrame && GeoRefsManager && GeoRefsManager->GetVRMoon())
	{
		const FVector RoverPos   = GetActorLocation();
		const FVector MoonCenter = GeoRefsManager->GetVRMoon()->GetActorLocation();
		const FVector DownDir    = (MoonCenter - RoverPos).GetSafeNormal();
		const FVector TraceEnd   = RoverPos + DownDir * 5000000.0f;

		FHitResult Hit;
		// bTraceComplex = true: Cesium tiles only have per-triangle (complex) collision.
		FCollisionQueryParams Params(FName(TEXT("RoverGroundProbe")), /*bTraceComplex=*/true, this);
		const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, RoverPos, TraceEnd, ECC_WorldStatic, Params);

		if (bHit)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Master][SERVER] GroundProbe HIT: Actor=%s Comp=%s Dist=%.1f"),
				*GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()), Hit.Distance);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[Master][SERVER] GroundProbe MISS: no collision under rover (tiles not streamed / no physics mesh / collision channel mismatch)."));
		}
	}

	if (!PuppetRover || !GeoRefsManager)
	{
		return;
	}

	// Run transform of puppet on server
	if (!HasAuthority())
	{
		// Position
		const FVector MasterWorldPos = GetActorLocation();
		const FVector MasterLocalPos_VRMoon_UE = GeoRefsManager->GetVRMoon()->GetTransform().InverseTransformPosition(MasterWorldPos);
		PuppetRover->SetActorRelativeLocation(MasterLocalPos_VRMoon_UE);

		// Orientation
		const FQuat MasterOrientation_World = GetActorQuat();
		const FQuat MasterOrientation_Local = GeoRefsManager->GetVRMoon()->GetActorQuat().Inverse() * MasterOrientation_World;
		PuppetRover->SetActorRelativeRotation(MasterOrientation_Local);	
	}

	if (bLogThisFrame)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Puppet][%s] Tick: MasterWorld=%s | RelLoc(VRlocal)=%s | PuppetWorld=%s"),
			Net, *GetActorLocation().ToString(), *GeoRefsManager->GetVRMoon()->GetTransform().InverseTransformPosition(GetActorLocation()).ToString(), *PuppetRover->GetActorLocation().ToString());
	}
}

FVector AMasterRover::GetLocalPos_UE() const
{
	const FVector WorldPos_UE = this->GetActorLocation();
	const FVector LocalPos_UE = GeoRefsManager->GetVRMoon()->GetTransform().InverseTransformPositionNoScale(WorldPos_UE);
	
	return LocalPos_UE;
}

APuppetRover* AMasterRover::GetPuppetRover()
{
	return PuppetRover;
}
