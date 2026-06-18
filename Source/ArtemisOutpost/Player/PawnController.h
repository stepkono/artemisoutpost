// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PawnAR.h"
#include "ACharVR.h"
#include "GameFramework/PlayerController.h"
#include "Rover/AMasterRover.h"
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
	
	UFUNCTION(BlueprintCallable, Category = "GeoReferences Manager")
	void SetGeoRefsManager(AGeoRefsManager* InGeoRefsManager); 
	
protected: 
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	
private: 
	UFUNCTION()
	void SpawnVRPlayer(); 
	
protected: 
	UPROPERTY(Replicated, BlueprintReadOnly)
	APawnAR* ARPawn; 
	
	UPROPERTY(Replicated, BlueprintReadOnly)
	ACharVR* VRPawn; 
	
	UPROPERTY(Replicated, BlueprintReadOnly)
	AMasterRover* MasterRover;
	
	UPROPERTY(BlueprintReadWrite, Category = "VR Position")
	FVector GeodeticPos = FVector(90.0f, 0.0f, 10.0f);
	
	UPROPERTY(BlueprintReadOnly, Category = "GeoRefs Manager")
	AGeoRefsManager* GeoRefsManager;
};
