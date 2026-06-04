// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CesiumGeoreference.h"
#include "GameFramework/Actor.h"
#include "GeoRefsManager.generated.h"

UCLASS()
class ARTEMISOUTPOST_API AGeoRefsManager : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AGeoRefsManager();
	
	virtual void Tick(float DeltaTime) override;
	
	UFUNCTION(BlueprintCallable, Category="AR Mooon")
	ACesiumGeoreference* GetARMoon(); 
	
	UFUNCTION(BlueprintCallable, Category="VR Mooon")
	ACesiumGeoreference* GetVRMoon();
	
	UFUNCTION(BlueprintCallable, Category="VR Moon")
	FVector UECoordsToVRMoonCoords(FVector& WorldCoords);
	
	UFUNCTION(BlueprintCallable, Category="VR Moon")
	FVector VRMoonCoordsToUECoords(FVector& LatLonHeightCoords); 
	
	UFUNCTION(BlueprintCallable, Category="AR Moon")
	FVector UECoordsToARMoonCoords(FVector WorldCoords); 
	
	UFUNCTION(BlueprintCallable, Category="AR Moon");
	FVector ARMoonCoordsToUECoords(FVector& LatLonHeightCoords); 

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

private: 
	UPROPERTY()
	ACesiumGeoreference* VRMoon; 
	
	UPROPERTY()
	ACesiumGeoreference* ARMoon;
};
