// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Cesium3DTileset.h"
#include "ArtemisOutpost/Cesium/GeoRefsManager.h"
#include "ArtemisOutpost/Miscellaneous/DataTypes.h"
#include "Components/ActorComponent.h"
#include "MoonDataManager.generated.h"

class UMoonTexturer;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), Blueprintable)
class ARTEMISOUTPOST_API UMoonDataManager : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UMoonDataManager();
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
							   FActorComponentTickFunction* ThisTickFunction) override;
	
	// Authoritative: runs on the server (invoked via APawnController::ServerAddAreaScan, since the
	// GameState is server-owned and cannot receive a client Server RPC). Replicates via AreaScans.
	UFUNCTION(BlueprintCallable, Category = "Area Scan")
	void AddNewAreaScan(FAreaScan AreaScan);
	
	UFUNCTION(BlueprintCallable, Category = "Moon Data")
	bool IsPositionExploredARMoon(FVector& GeoPosition, bool bForARMoon);
	
	UFUNCTION()
	void UpdateFogOfWarTexture(); 
	
protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	
private: 
	UFUNCTION()
	void OnRep_UpdateForOfWar();
	
private:
	UPROPERTY()
	UMoonTexturer* MoonTexturer;

	UPROPERTY()
	AGeoRefsManager* GeoRefsManager;
	
	UPROPERTY()
	ACesium3DTileset* ARMoonTileSet; 
	
	UPROPERTY(ReplicatedUsing=OnRep_UpdateForOfWar)
	TArray<FAreaScan> AreaScans;
	
	UPROPERTY()
	float ScanningRadius;
};
