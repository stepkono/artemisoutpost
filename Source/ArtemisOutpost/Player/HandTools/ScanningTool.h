// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "HandToolBase.h"
#include "ScanningTool.generated.h"

UCLASS()
class ARTEMISOUTPOST_API AScanningTool : public AHandToolBase
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AScanningTool();
	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
	UFUNCTION(BlueprintCallable, Category = "Scanning Tool")
	void SetScannerOn(bool bScannerOn); 

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

private: 
	void ScanSurface(); 
	
	void ResetScanningArea();
	
protected:
	UPROPERTY(EditAnywhere, Category = "Scanning Tool")
	float ScanningRadius = 100; 
	
	UPROPERTY(EditAnywhere, Category = "Scanning Tool")
	float ScanningDistance = 200;
	
	UPROPERTY(EditAnywhere, Category = "Scanning Tool")
	UMaterialParameterCollection* SurfaceScannerCollection;
	
private: 
	UPROPERTY()
	bool bIsScannerOn; 
	
	UPROPERTY()
	USceneComponent* Muzzle; 
};
