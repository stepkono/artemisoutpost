// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PawnAR.h"
#include "ACharVR.h"
#include "ArtemisOutpost/AMasterRover.h"
#include "GameFramework/PlayerController.h"
#include "PawnController.generated.h"

/**
 * 
 */
UCLASS()
class ARTEMISOUTPOST_API APawnController : public APlayerController
{
	GENERATED_BODY()
	
public: 
	UFUNCTION(BlueprintCallable, Category = "AR/VR Pawns")
	void InitializePawns(ACharVR* VRPlayer,  AMasterRover* RoverPuppet); 
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
protected: 
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	
private: 
	UFUNCTION()
	void SpawnARPlayer(); 
	
	UFUNCTION()
	void SpawnVRPlayer(); 
	
	UFUNCTION()
	void SpawnPuppetPawns(APawn* InPuppetPawn);
	
protected: 
	UPROPERTY(Replicated, BlueprintReadOnly)
	APawnAR* ARPawn; 
	
	UPROPERTY(Replicated, BlueprintReadOnly)
	ACharVR* VRPawn; 
	
	UPROPERTY(Replicated, BlueprintReadOnly)
	AMasterRover* MasterRover;
	
	UPROPERTY(BlueprintReadWrite, Category = "VR Position")
	FVector GeodeticPos = FVector(90.0f, 0.0f, 10.0f);
	
private: 
	UPROPERTY()
	bool bInitialPosses = true; 
};
