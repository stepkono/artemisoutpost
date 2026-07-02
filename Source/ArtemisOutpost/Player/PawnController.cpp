// Fill out your copyright notice in the Description page of Project Settings.


#include "PawnController.h"
#include "Net/UnrealNetwork.h"

void APawnController::BeginPlay()
{
	Super::BeginPlay();
	
	UArtemisGameInstance* GI = Cast<UArtemisGameInstance>(GetGameInstance());
	const FString CachedUPID = GI->GetUPID(); 
	if (CachedUPID.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("PawnController: UPID returned empty string."));
		return; 
	}
	
	UPID = CachedUPID;
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
