// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "HandToolBase.h"
#include "ArtemisOutpost/MiniGames/General/MinigameTypes.h"
#include "ArtemisOutpost/Moon/MoonResources/MoonResourcesManager.h"
#include "AResourceTool.generated.h"

UCLASS()
class ARTEMISOUTPOST_API AResourceTool : public AHandToolBase
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AResourceTool();

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Scanning Tool")
	void SetScannerOn(bool bScannerOn);
	
	UFUNCTION(BlueprintCallable, Category = "Scanning Tool")
	void SetScannerMode(EScanMode ScanMode); 

	// Enter scanning for a mode (called from the HUD's Scanning tiles). Activates the tool; the scanner
	// itself is turned on/off with the trigger (ExecuteAction / EndAction = hold-to-scan).
	UFUNCTION(BlueprintCallable, Category = "Scanning Tool")
	void BeginScan(EScanMode Mode);

	UFUNCTION(BlueprintPure, Category = "Scanning Tool")
	EScanMode GetCurrentScanMode() const { return CurrentScanMode; }

	UFUNCTION(BlueprintPure, Category = "Scanning Tool")
	bool IsScanning() const { return bIsScannerOn; }

	// AHandToolBase — trigger down/up (hold-to-scan), + holster safety.
	virtual void ExecuteAction() override;
	virtual void EndAction() override;
	virtual void DeactivateTool() override;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

private:
	void SeeThroughSurface(FVector& OutHitLocation);
	void ResetScanningArea();
	void DiscoverResource(FVector& HitLocation);
	void MineResource(FVector& HitLocation, float DeltaSeconds);

protected:
	UPROPERTY(EditAnywhere, Category = "Scanning Tool")
	float ScanningRadius = 100;

	UPROPERTY(EditAnywhere, Category = "Scanning Tool")
	float ScanningDistance = 200;

	// Mining speed: fraction (0..1) of a vein sample mined per second while under the scan circle.
	// 1.0 => a sample takes ~1 s of full coverage to disappear.
	UPROPERTY(EditAnywhere, Category = "Scanning Tool")
	float MiningRatePerSecond = 1.f;

	UPROPERTY(EditAnywhere, Category = "Scanning Tool")
	UMaterialParameterCollection* SurfaceScannerCollection;

	// Which scan the trigger runs (set by BeginScan from the HUD). Surface uses the aim trace below;
	// Area (environment around the player) is a future addition to ScanSurface / a new pass.
	UPROPERTY(BlueprintReadOnly, Category = "Scanning Tool")
	EScanMode CurrentScanMode = EScanMode::Surface;

private:
	UPROPERTY()
	UMoonResourcesManager* VeinSubsystem;
	
	UPROPERTY()
	bool bIsScannerOn;

	UPROPERTY()
	USceneComponent* Muzzle;
};
