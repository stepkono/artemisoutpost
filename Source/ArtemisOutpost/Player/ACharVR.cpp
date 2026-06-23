// Fill out your copyright notice in the Description page of Project Settings.


#include "ACharVR.h"
#include "Cesium3DTileset.h"
#include "EngineUtils.h"
#include "ArtemisOutpost/Miscellaneous/NetUtils.h"


// Sets default values
ACharVR::ACharVR()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	bAlwaysRelevant = true;
}

// Called when the game starts or when spawned
void ACharVR::BeginPlay()
{
	Super::BeginPlay();

	// VR-moon tileset caching is client-only (used to show/hide the player's view).
	// The server host doesn't render and must not depend on it.
	if (ArtemisNet::IsServerHost(GetNetMode()))
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("ACharVR: Failed to get world."));
		return;
	}

	for (const auto TileSet : TActorRange<ACesium3DTileset>(World))
	{
		if (TileSet->ActorHasTag(FName("DEFAULT_TILESET")))
		{
			VRTileSet = TileSet;
			break; 
		}
	}
	if (!VRTileSet)
	{
		UE_LOG(LogTemp, Error, TEXT("ACharVR: Failed to initialize VR Moon tileset.")); 
	}
}

// Called every frame
void ACharVR::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// ---- Client-side VR-moon collision probe ----
	// The VR character stands on the VR moon and the client has a real player camera
	// driving Cesium streaming, so this tells us whether the VR moon has collision on
	// the client. Compared with the server-side rover GroundProbe (same VR moon):
	//   client HIT + server MISS  -> server-only issue (no camera/streaming on the server)
	//   client MISS + server MISS -> general problem (Create Physics Meshes / channel / LOD)
	// Client-only: must not run on the listen-server host (its char is locally controlled too).
	if (!(IsLocallyControlled() && ArtemisNet::IsClientContext(GetNetMode())))
	{
		return;
	}

	DebugProbeAccumulator += DeltaTime;
	if (DebugProbeAccumulator < 5.0f)
	{
		return;
	}
	DebugProbeAccumulator = 0.0f;

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// VR moon georeference = the Cesium georeference that is NOT the AR moon.
	ACesiumGeoreference* VRGeo = nullptr;
	for (TActorIterator<ACesiumGeoreference> It(World); It; ++It)
	{
		if (*It && !(*It)->ActorHasTag(FName("AR_GEOREF")))
		{
			VRGeo = *It;
			break;
		}
	}
	if (!VRGeo)
	{
		UE_LOG(LogTemp, Error, TEXT("[VRChar][CLIENT] VRMoonProbe: no non-AR georeference found."));
		return;
	}

	const FVector CharPos = GetActorLocation();
	const FVector Center  = VRGeo->GetActorLocation();          // VR moon centre (planet centre)
	const FVector Down    = (Center - CharPos).GetSafeNormal(); // toward moon centre = "down" on a globe
	const FVector End     = CharPos + Down * 500000000.0f;      // 5e8 UE units, long enough to cross the moon

	FHitResult Hit;
	FCollisionQueryParams Params(FName(TEXT("VRCharVRMoonProbe")), /*bTraceComplex=*/true, this);
	const bool bHit = World->LineTraceSingleByChannel(Hit, CharPos, End, ECC_WorldStatic, Params);

	if (bHit)
	{
		UE_LOG(LogTemp, Warning, TEXT("[VRChar][CLIENT] VRMoonProbe HIT: Actor=%s Comp=%s Dist=%.1f | CharPos=%s"),
			*GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()), Hit.Distance, *CharPos.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[VRChar][CLIENT] VRMoonProbe MISS | CharPos=%s | VRGeo=%s"),
			*CharPos.ToString(), *VRGeo->GetName());
	}
}

// Called to bind functionality to input
void ACharVR::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void ACharVR::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();
	
	UE_LOG(LogTemp, Log, TEXT("ACharVR: NotifyControllerChanged"));
	
	
	// Check locally 
	if (GetLocalRole() == ROLE_AutonomousProxy)
	{
		const AController* CurrentController = GetController();
		
		if (!VRTileSet)
		{
			UE_LOG(LogTemp, Error, TEXT("ACharVR: Tileset it not set."))
			return; 
		}
		
		// UNPOSSESSED
		if (CurrentController == nullptr)
		{
			UE_LOG(LogTemp, Log, TEXT("ACharVR: UNPOSSESSED"));
			//VRTileSet->SetActorHiddenInGame(true);
		}
		//POSSESSED
		else
		{
			UE_LOG(LogTemp, Log, TEXT("ACharVR: POSSESSED"));
			//VRTileSet->SetActorHiddenInGame(false);
		}		
	}
}

ACesium3DTileset* ACharVR::GetVRTileset()
{
	return VRTileSet;
}

void ACharVR::SetGeoRefsManager(AGeoRefsManager* InManager)
{
	GeoRefsManager = InManager;
}
