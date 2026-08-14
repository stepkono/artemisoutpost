// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PawnAR.h"
#include "ACharVR.h"
#include "GameFramework/PlayerController.h"
#include "Rover/AMasterRover.h"
#include "PawnController.generated.h"

class UMinigamePlayerController;

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

	// The per-player minigame controller (transport + screen-UI lifecycle). Its own component
	// (§9) so this controller stays lean.
	UFUNCTION(BlueprintPure, Category = "Minigame")
	UMinigamePlayerController* GetMinigamePlayerController() const;

	// Set server-side from the ?UPID= login option (see AServerGameMode::InitNewPlayer). The client
	// sets its own UPID from the save/GameInstance in BeginPlay.
	void SetUPID(const FString& InUPID);
	
protected: 
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;

protected: 
	UPROPERTY(BlueprintReadOnly, Category = "Player ID")
	FString UPID; 
	
	UPROPERTY(Replicated, BlueprintReadOnly)
	APawnAR* ARPawn; 
	
	UPROPERTY(Replicated, BlueprintReadOnly)
	ACharVR* VRPawn; 
	
	UPROPERTY(Replicated, BlueprintReadOnly)
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
