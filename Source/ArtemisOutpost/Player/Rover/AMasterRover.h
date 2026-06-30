// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "ArtemisOutpost/Cesium/GeoRefsManager.h"
#include "ArtemisOutpost/GameData/ArtemisGameState.h"
#include "AMasterRover.generated.h"

class APuppetRover;

/**
 * 
 */
UCLASS()
class ARTEMISOUTPOST_API AMasterRover : public AWheeledVehiclePawn
{
	GENERATED_BODY()
	
public:
	AMasterRover();
	
	virtual void BeginPlay() override;
	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
	UFUNCTION()
	FVector GetLocalPos_UE() const; 
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	UFUNCTION(BlueprintCallable, Category = "Puppet Rover")
	APuppetRover* GetPuppetRover(); 
	
	UFUNCTION(BlueprintCallable, Category = "GeoRefsManager")
	void SetGeoRefsManager(AGeoRefsManager* InManager); 
	
private: 
	UFUNCTION()
	void HandleControls(FControlCommand& ControlCommand); 
	
protected:
	UPROPERTY(BlueprintReadOnly, Category="GeoRefsManager")
	AGeoRefsManager* GeoRefsManager;
	
	UPROPERTY(BlueprintReadWrite, Category="Puppet Rover")
	APuppetRover* PuppetRover;
	
	UPROPERTY(BlueprintReadOnly, Category="Game State")
	AArtemisGameState* GameState;
	
private:
	UPROPERTY()
	FVector StartLocalPosition_UE;

	// Accumulates DeltaTime so Tick can log roughly every LogIntervalSeconds.
	float LogTimeAccumulator = 0.0f;
	static constexpr float LogIntervalSeconds = 5.0f;

	// Per-tick position tracking to quantify the jitter/bounce without per-frame log spam:
	// every tick we measure |dPos| and keep the PEAK; the throttled log reports that peak.
	FVector LastTickPos = FVector::ZeroVector;
	bool bHasLastTickPos = false;
	float MaxTickDelta = 0.0f;
};
