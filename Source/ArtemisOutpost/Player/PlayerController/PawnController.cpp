// Fill out your copyright notice in the Description page of Project Settings.


#include "PawnController.h"

#include "EngineUtils.h"
#include "ArtemisOutpost/Moon/MoonScannedArea/MoonScannedAreaManager.h"
#include "Net/UnrealNetwork.h"
#include "ArtemisOutpost/Networking/ClientServerConnection/NetUtils.h"
#include "ArtemisOutpost/GameData/ArtemisGameState.h"
#include "ArtemisOutpost/GameData/ServerGameMode.h"
#include "ArtemisOutpost/Moon/Cesium/CustomCesiumCameraManager.h"
#include "ArtemisOutpost/Player/PlayerController/PlayerControllerComponents/MiniGameInteraction/MinigamePlayerController.h"
#include "ArtemisOutpost/Moon/MoonResources/MoonResourcesManager.h"
#include "ArtemisOutpost/StudyData/Providers/PlayerActionProvider/PlayerActionProvider.h"
#include "ArtemisOutpost/GameData/ArtemisPlayerState.h"

APawnController::APawnController()
{
	MinigameController = CreateDefaultSubobject<UMinigamePlayerController>(TEXT("MinigameController"));
}

UMinigamePlayerController* APawnController::GetMinigamePlayerController() const
{
	return MinigameController;
}

void APawnController::BeginPlay()
{
	Super::BeginPlay();

	// UPID is the persistent player identity. On the SERVER it is set from the ?UPID= login option in
	// AServerGameMode::InitNewPlayer, so we must NOT overwrite it here with the server machine's own
	// GameInstance value. Only the client reads its identity from its local save/GameInstance.
	if (!ArtemisNet::IsClientContext(GetNetMode()))
	{
		UE_LOG(LogMinigame, Log, TEXT("[UPID] %s (%s): server-side controller, identity comes from the ?UPID= login option (currently '%s')."),
			*GetName(), ArtemisNet::RoleName(GetNetMode()), *UPID);
		return;
	}

	const UArtemisGameInstance* GI = Cast<UArtemisGameInstance>(GetGameInstance());
	if (!GI)
	{
		UE_LOG(LogMinigame, Error, TEXT("[UPID] %s (%s): GameInstance is not a UArtemisGameInstance -> this client has NO identity. Every minigame join from here will be refused."),
			*GetName(), ArtemisNet::RoleName(GetNetMode()));
		return;
	}

	const FString CachedUPID = GI->GetUPID();
	if (CachedUPID.IsEmpty())
	{
		UE_LOG(LogMinigame, Error, TEXT("[UPID] %s (%s): GameInstance returned an EMPTY UPID -> this client has NO identity. RefreshLocalUI can never match a slot, so no minigame View will ever open here."),
			*GetName(), ArtemisNet::RoleName(GetNetMode()));
		return;
	}

	UPID = CachedUPID;

	// The value the server holds for this player comes from the ?UPID= login option, which the
	// client passes on connect. If THIS value and the server's [UPID] line for the same player differ,
	// the server grants slots to a name this client never compares against -> the View never opens.
	UE_LOG(LogMinigame, Log, TEXT("[UPID] %s (%s): local identity from GameInstance = '%s'. Must equal the ?UPID= the server logged for this player."),
		*GetName(), ArtemisNet::RoleName(GetNetMode()), *UPID);
}

void APawnController::SetUPID(const FString& InUPID)
{
	UE_LOG(LogMinigame, Log, TEXT("[UPID] %s (%s): identity set to '%s' (was '%s')."),
		*GetName(), ArtemisNet::RoleName(GetNetMode()), *InUPID, *UPID);
	UPID = InUPID;

	// Server: stamp the identity onto the replicated PlayerState so every client can map it back.
	if (HasAuthority())
	{
		if (AArtemisPlayerState* PS = GetArtemisPlayerState())
		{
			PS->ServerSetUPID(UPID);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[Cues] %s: SetUPID before the PlayerState exists or PlayerStateClass is not AArtemisPlayerState. Check the GameMode's PlayerStateClass."), *GetName());
		}
	}
}

AArtemisPlayerState* APawnController::GetArtemisPlayerState() const
{
	return GetPlayerState<AArtemisPlayerState>();
}

void APawnController::SetXRMode(EXRMode Mode)
{
	CurrentXRMode = Mode;

	if (HasAuthority())
	{
		ServerReportXRMode_Implementation(Mode);
	}
	else
	{
		ServerReportXRMode(Mode);
	}
}

void APawnController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(APawnController, ARPawn);
	DOREPLIFETIME(APawnController, VRPawn);
	DOREPLIFETIME(APawnController, MasterRover);
	DOREPLIFETIME(APawnController, GeoRefsManager); 
	DOREPLIFETIME(APawnController, bIsPlayingMinigame);
}

void APawnController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	SetViewTarget(InPawn); // TODO: do we have to do that? 
	
	// The mode switch spawns a NEW AR pawn on every switch to AR, so always track the current one.
	if (APawnAR* AR = Cast<APawnAR>(InPawn))
	{
		if (ARPawn != AR)
		{
			UE_LOG(LogTemp, Log, TEXT("PawnController: AR Pawn was possessed and set (%s)."), *AR->GetName());
		}
		ARPawn = AR;
	}

	// Server: let every peer resolve "this actor belongs to that player" (pointing at players).
	if (HasAuthority() && ARPawn)
	{
		if (AArtemisPlayerState* PS = GetArtemisPlayerState())
		{
			PS->ServerSetPawns(nullptr, ARPawn, nullptr);
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

	if (HasAuthority())
	{
		if (AArtemisPlayerState* PS = GetArtemisPlayerState())
		{
			PS->ServerSetPawns(VRPawn, nullptr, MasterRover);
		}
	}

	AServerGameMode* ServerGameMode = Cast<AServerGameMode>(GetWorld()->GetAuthGameMode());
	if (!ServerGameMode)
	{
		UE_LOG(LogTemp, Error, TEXT("PawnController: Failed to cast GameMode as ServerGameMode. Aborting."));
		return;
	}
	
	// Init GeoRef on the pawns
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

	UMoonScannedAreaManager* MDM = AGS->FindComponentByClass<UMoonScannedAreaManager>();
	if (!MDM)
	{
		UE_LOG(LogTemp, Error, TEXT("[PawnController] ServerAddAreaScan: No MoonDataManager on GameState."));
		return;
	}

	MDM->AddNewAreaScan(Scan);
}

void APawnController::ServerReportVeinDiscovered_Implementation(UResourceVeinSpline* Vein, const TArray<int32>& Indices)
{
	// Runs on the server. Route to the authoritative resource subsystem.
	if (UMoonResourcesManager* Subsys = GetWorld() ? GetWorld()->GetSubsystem<UMoonResourcesManager>() : nullptr)
	{
		Subsys->ServerReportDiscovered(Vein, Indices);
	}
}

void APawnController::ServerReportVeinMined_Implementation(UResourceVeinSpline* Vein, const TArray<int32>& Indices)
{
	if (UMoonResourcesManager* Subsys = GetWorld() ? GetWorld()->GetSubsystem<UMoonResourcesManager>() : nullptr)
	{
		Subsys->ServerReportMined(Vein, Indices);
	}
}

void APawnController::ServerReportHmdState_Implementation(bool bWorn)
{
	// Runs on the server. UPID here is the authoritative value set from the ?UPID= login option, so
	// the client never has to send it. Route to the player-action provider for aggregation.
	UE_LOG(LogTemp, Warning, TEXT("[HMD] Server RPC received: worn=%d, UPID=%s"), bWorn ? 1 : 0, *UPID);

	// Context: a doffed HMD puts the player into "R" until it is donned again.
	if (AArtemisPlayerState* PS = GetArtemisPlayerState())
	{
		PS->ServerSetHmdWorn(bWorn);
	}

	UPlayerActionProvider* Provider = GetWorld() ? GetWorld()->GetSubsystem<UPlayerActionProvider>() : nullptr;
	if (!Provider)
	{
		UE_LOG(LogTemp, Error, TEXT("[HMD] Server RPC: no UPlayerActionProvider subsystem — event dropped."));
		return;
	}
	Provider->ServerReportHmdState(UPID, bWorn);
}

ACharVR* APawnController::GetVRPawn() const
{
	return VRPawn;
}

void APawnController::OnRep_VRPawn()
{
	VRPawnInitialized();
}

void APawnController::OnRep_MasterRover()
{
	RegisterProxyCam();
}

void APawnController::RegisterProxyCam() const
{
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<ACustomCesiumCameraManager> It(World); It; ++It)
		{
			ACustomCesiumCameraManager* CameraManager = *It;
			CameraManager->AddNewMasterRover(MasterRover);
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("PawnController: Failed to get world on RegisterProxyCam."));
	}
}

void APawnController::SetIsPlayingMiniGame(const bool IsInGame)
{
	bIsPlayingMinigame = IsInGame;
}

void APawnController::OnNetCleanup(UNetConnection* Connection)
{
	if (Cast<ACharVR>(GetPawn()))
	{
		UnPossess();
	}
	
	Super::OnNetCleanup(Connection);
}

void APawnController::ShouldActivateVRCharPuppet(bool bShouldActivate) const
{
	if (!VRPawn)
	{
		return; 
	}
	
	if (CurrentXRMode == VR)
	{
		VRPawn->ShouldActivateARPuppet(bShouldActivate);
	}
}

// ---- Awareness cues: client -> server -> PlayerState ----

void APawnController::ServerReportXRMode_Implementation(EXRMode Mode)
{
	if (AArtemisPlayerState* PS = GetArtemisPlayerState())
	{
		PS->ServerSetXRMode(Mode);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cues] %s: ServerReportXRMode without an AArtemisPlayerState — context not updated."), *GetName());
	}
}

void APawnController::ServerSetTalking_Implementation(bool bTalking)
{
	if (AArtemisPlayerState* PS = GetArtemisPlayerState())
	{
		PS->ServerSetTalking(bTalking);
	}
}

void APawnController::ServerSetPointing_Implementation(EPointingHand Hand, bool bPointing)
{
	if (AArtemisPlayerState* PS = GetArtemisPlayerState())
	{
		PS->ServerSetPointing(Hand, bPointing);
	}
}

void APawnController::ServerReportPointerTarget_Implementation(FPointingTarget Target)
{
	if (AArtemisPlayerState* PS = GetArtemisPlayerState())
	{
		PS->ServerSetPointerTarget(Target);
	}
}

void APawnController::ServerReportGazeTarget_Implementation(FPointingTarget Target)
{
	if (AArtemisPlayerState* PS = GetArtemisPlayerState())
	{
		PS->ServerSetGazeTarget(Target);
	}
}

void APawnController::ServerReportToolActivity_Implementation(EToolActivity Tool)
{
	if (AArtemisPlayerState* PS = GetArtemisPlayerState())
	{
		PS->ServerSetToolActivity(Tool);
	}
}
