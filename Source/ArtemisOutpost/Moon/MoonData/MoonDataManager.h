// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArtemisOutpost/Cesium/GeoRefsManager.h"
#include "ArtemisOutpost/Miscellaneous/DataTypes.h"
#include "Components/ActorComponent.h"
#include "MoonDataManager.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ARTEMISOUTPOST_API UMoonDataManager : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UMoonDataManager();
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
							   FActorComponentTickFunction* ThisTickFunction) override;
	
	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Area Scan")
	void AddNewAreaScan(FAreaScan AreaScan);
	
	UFUNCTION(BlueprintCallable, Category = "Moon Data")
	bool IsPositionExploredARMoon(FVector& GeoPosition, bool bForARMoon); 
	
protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	
private: 
	UFUNCTION()
	void OnRep_UpdateForOfWar();
	
	AGeoRefsManager* GeoRefsManager;

private: 
	UPROPERTY(ReplicatedUsing=OnRep_UpdateForOfWar)
	TArray<FAreaScan> AreaScans;
	
	UPROPERTY()
	float ScanningRadius;
};
