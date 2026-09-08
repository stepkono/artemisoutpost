// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArtemisOutpost/MiniGames/General/MinigameTypes.h"
#include "ArtemisOutpost/Player/PawnAR/PawnAR.h"
#include "ArtemisOutpost/Player/PawnVR/ACharVR.h"
#include "GameFramework/PlayerController.h"
#include "ArtemisOutpost/Player/Rover/AMasterRover.h"
#include "PawnController.generated.h"

class UMinigamePlayerController;
class UResourceVeinSpline;

/**
 *
 */
UCLASS()
class ARTEMISOUTPOST_API APawnController : public APlayerController
{
	GENERATED_BODY()

public:
	APawnController();

	UFUNCTION(BlueprintCallable, Category = "AR/VR Pawns")
	void InitializePawns(ACharVR* InVRChar,  AMasterRover* InMasterRover); 
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	UFUNCTION(BlueprintCallable, Category = "GeoReferences Manager")
	void SetGeoRefsManager(AGeoRefsManager* InGeoRefsManager); 
	
	UFUNCTION(BlueprintCallable, Category = "XR Mode")
	TEnumAsByte<EXRMode> GetXRMode() const;
	
	UFUNCTION(BlueprintCallable, Category = "Player ID")
	FString GetPlayerUPID() const;
	
	UFUNCTION(BlueprintCallable, Category = "AR/VR Pawns")
	ACharVR* GetVRPawn() const;

	// Client -> server entry point for adding an area scan. The GameState (which owns MoonDataManager)
	// is server-owned, so a client Server RPC must originate on this client-owned PlayerController.
	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Area Scan")
	void ServerAddAreaScan(FAreaScan Scan);

	// Client -> server: report vein samples the client detected locally. Same reason as above —
	// the resource state lives on server-owned objects, so client reports must originate on this
	// client-owned controller. These fire only on per-sample flips (event-gated), not per tick.
	UFUNCTION(Server, Reliable, Category = "Resource Vein")
	void ServerReportVeinDiscovered(UResourceVeinSpline* Vein, const TArray<int32>& Indices);

	UFUNCTION(Server, Reliable, Category = "Resource Vein")
	void ServerReportVeinMined(UResourceVeinSpline* Vein, const TArray<int32>& Indices);

	// Client -> server: report HMD worn-state changes (doff/don). The client-side proximity signal
	// originates in UConnectionLifecycleSubsystem and is sent through this client-owned controller;
	// the server fills in the player identity from its authoritative UPID, so only the worn flag
	// travels on the wire.
	UFUNCTION(Server, Reliable, Category = "HMD")
	void ServerReportHmdState(bool bWorn);

	// The per-player minigame controller (transport + screen-UI lifecycle). Its own component
	// (§9) so this controller stays lean.
	UFUNCTION(BlueprintPure, Category = "Minigame")
	UMinigamePlayerController* GetMinigamePlayerController() const;

	// Set server-side from the ?UPID= login option (see AServerGameMode::InitNewPlayer). The client
	// sets its own UPID from the save/GameInstance in BeginPlay.
	void SetUPID(const FString& InUPID);
	
	UFUNCTION(BlueprintImplementableEvent, Category = "Minigame")
	void ActivateMiniGameInput(EMiniGameType MiniGameType);
	
	UFUNCTION(BlueprintImplementableEvent, Category = "Minigame")
	void DeactivateMiniGameInput();
	
	void SetIsPlayingMiniGame(const bool IsInGame);
	
protected: 
	virtual void BeginPlay() override;
	
	virtual void OnPossess(APawn* InPawn) override;
	
	virtual void OnNetCleanup(UNetConnection* Connection) override;
	
	UFUNCTION(BlueprintImplementableEvent, Category = "VR Pawn")
	void VRPawnInitialized();
	
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Mini Game")
	bool bIsPlayingMinigame = false;
	
private: 
	UFUNCTION()
	void OnRep_VRPawn();
	
	UFUNCTION()
	void OnRep_MasterRover(); 
	
	UFUNCTION()
	void RegisterProxyCam() const; 

protected: 
	UPROPERTY(BlueprintReadOnly, Category = "Player ID")
	FString UPID; 
	
	UPROPERTY(Replicated, BlueprintReadWrite)
	APawnAR* ARPawn; 
	
	UPROPERTY( ReplicatedUsing=OnRep_VRPawn, BlueprintReadWrite)
	ACharVR* VRPawn; 
	
	UPROPERTY(ReplicatedUsing=OnRep_MasterRover, BlueprintReadOnly)
	AMasterRover* MasterRover;
	
	UPROPERTY(BlueprintReadWrite, Category = "VR Position")
	FVector GeodeticPos = FVector(90.0f, 0.0f, 10.0f);
	
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "GeoRefs Manager")
	AGeoRefsManager* GeoRefsManager;
	
	UPROPERTY(BlueprintReadWrite, Category = "XR Mode")
	TEnumAsByte<EXRMode> CurrentXRMode = EXRMode::AR;
	
	UPROPERTY(BlueprintReadWrite, Category = "XR Mode")
	float CachedWTMValue = 100;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Minigame")
	UMinigamePlayerController* MinigameController;
};
