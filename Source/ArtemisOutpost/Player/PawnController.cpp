// Fill out your copyright notice in the Description page of Project Settings.


#include "PawnController.h"

#include "ArtemisOutpost/Moon/MoonData/MoonDataManager.h"
#include "Net/UnrealNetwork.h"
#include "ArtemisOutpost/Networking/ClientServerConnection/NetUtils.h"
#include "ArtemisOutpost/GameData/ArtemisGameState.h"

void APawnController::BeginPlay()
{
	Super::BeginPlay();

	// UPID is the persistent player identity. On the SERVER it is set from the ?UPID= login option in
	// AServerGameMode::InitNewPlayer, so we must NOT overwrite it here with the server machine's own
	// GameInstance value. Only the client reads its identity from its local save/GameInstance.
	if (!ArtemisNet::IsClientContext(GetNetMode()))
	{
		return;
	}

	const UArtemisGameInstance* GI = Cast<UArtemisGameInstance>(GetGameInstance());
	if (!GI)
	{
		UE_LOG(LogTemp, Error, TEXT("PawnController: GameInstance is not a UArtemisGameInstance."));
		return;
	}

	const FString CachedUPID = GI->GetUPID();
	if (CachedUPID.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("PawnController: UPID returned empty string."));
		return;
	}

	UPID = CachedUPID;
}

void APawnController::SetUPID(const FString& InUPID)
{
	UPID = InUPID;
}

void APawnController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(APawnController, ARPawn);
	DOREPLIFETIME(APawnController, VRPawn);
	DOREPLIFETIME(APawnController, MasterRover);
	DOREPLIFETIME(APawnController, GeoRefsManager); 
}

void APawnController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	SetViewTarget(InPawn); // TODO: do we have to do that? 
	
	if (!ARPawn)
	{
		if (APawnAR* AR = Cast<APawnAR>(InPawn))
		{
			ARPawn = AR;
		}
	}
}

void APawnController::InitializePawns(ACharVR* InVRChar, AMasterRover* InMasterRover)
{
	const TCHAR* Net = HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT");
	UE_LOG(LogTemp, Warning, TEXT("[Ctrl][%s] InitializePawns: VRPlayer=%s | RoverPuppet(Master)=%s"),
		Net, *GetNameSafe(InVRChar), *GetNameSafe(InMasterRover));

	if (!InVRChar)
	{
		UE_LOG(LogTemp, Error, TEXT("[Ctrl][%s] InitializePawns: VRPlayer is NULL. Aborting."), Net);
		return; 
	}
	if (!InMasterRover)
	{
		UE_LOG(LogTemp, Error, TEXT("[Ctrl][%s] InitializePawns: RoverPuppet (Master) is NULL. Aborting."), Net);
		return; 
	}
	
	VRPawn = InVRChar;
	MasterRover = InMasterRover;
	
	if (!GeoRefsManager)
	{
		UE_LOG(LogTemp, Error, TEXT("PawnController: GeoRefsManager is NULL. Aborting."))
		return;
	}
	
	VRPawn->SetGeoRefsManager(GeoRefsManager);
	MasterRover->SetGeoRefsManager(GeoRefsManager);
}

void APawnController::SetGeoRefsManager(AGeoRefsManager* InGeoRefsManager)
{
	GeoRefsManager = InGeoRefsManager;
}

TEnumAsByte<EXRMode> APawnController::GetXRMode() const 
{
	return CurrentXRMode; 
}

FString APawnController::GetPlayerUPID() const
{
	return UPID;
}

void APawnController::ServerAddAreaScan_Implementation(FAreaScan Scan)
{
	// Runs on the server. Forward to the authoritative MoonDataManager on the GameState.
	AArtemisGameState* AGS = GetWorld() ? GetWorld()->GetGameState<AArtemisGameState>() : nullptr;
	if (!AGS)
	{
		UE_LOG(LogTemp, Error, TEXT("[PawnController] ServerAddAreaScan: GameState is not AArtemisGameState."));
		return;
	}

	UMoonDataManager* MDM = AGS->FindComponentByClass<UMoonDataManager>();
	if (!MDM)
	{
		UE_LOG(LogTemp, Error, TEXT("[PawnController] ServerAddAreaScan: No MoonDataManager on GameState."));
		return;
	}

	MDM->AddNewAreaScan(Scan);
}
