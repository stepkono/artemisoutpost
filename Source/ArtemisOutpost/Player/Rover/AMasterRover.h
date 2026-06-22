// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "ArtemisOutpost/Cesium/GeoRefsManager.h"
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
	
protected:
	UPROPERTY(BlueprintReadWrite, Category="GeoRef")
	AGeoRefsManager* GeoRefsManager;
	
	UPROPERTY(BlueprintReadWrite, Category="Puppet Rover")
	APuppetRover* PuppetRover;
	
private:
	UPROPERTY()
	FVector StartLocalPosition_UE;

	// Accumulates DeltaTime so Tick can log roughly every LogIntervalSeconds.
	float LogTimeAccumulator = 0.0f;
	static constexpr float LogIntervalSeconds = 5.0f;
};
