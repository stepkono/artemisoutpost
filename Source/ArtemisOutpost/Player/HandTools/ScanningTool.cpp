// Fill out your copyright notice in the Description page of Project Settings.


#include "ScanningTool.h"

#include "Kismet/KismetMaterialLibrary.h"


// Sets default values
AScanningTool::AScanningTool()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void AScanningTool::BeginPlay()
{
	Super::BeginPlay();
	
	if (UResourceVeinSubsystem* RVS = GetWorld()->GetSubsystem<UResourceVeinSubsystem>())
	{
		VeinSubsystem = RVS;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ScanningTool: Failed to get ResourceVeinSubsystem."))
	}

	Muzzle = Cast<USceneComponent>(FindComponentByTag<UActorComponent>(FName("Muzzle")));
	if (!Muzzle)
	{
		UE_LOG(LogTemp, Error, TEXT("ScanningTool: Failed to get Muzzle."));
		return; 
	}

	if (!SurfaceScannerCollection)
	{
		UE_LOG(LogTemp, Error, TEXT("ScanningTool: SurfaceScannerCollection is not initialized."));
		return; 
	}
	
	UKismetMaterialLibrary::SetScalarParameterValue(GetWorld(), SurfaceScannerCollection, FName("ScanningRadius"), ScanningRadius);
}

// Called every frame
void AScanningTool::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!(Muzzle && SurfaceScannerCollection))
	{
		return;
	}

	FVector HitLocation = FVector::ZeroVector;
	SeeThroughSurface(HitLocation);
	if (HitLocation == FVector::ZeroVector)
	{
		return; 
	}
	
	if (CurrentScanMode == EScanMode::Surface)
	{
		DiscoverResource(HitLocation);
	}
	if (CurrentScanMode == EScanMode::Mining)
	{
		MineResource(HitLocation, DeltaTime);
	}
}

void AScanningTool::SetScannerOn(bool bScannerOn)
{
	bIsScannerOn = bScannerOn;
}

void AScanningTool::BeginScan(EScanMode Mode)
{
	CurrentScanMode = Mode;
	if (!bToolActive)
	{
		ActivateTool();
	}
}

void AScanningTool::ExecuteAction()
{
	// Trigger held -> scan.
	SetScannerOn(true);
}

void AScanningTool::EndAction()
{
	// Trigger released -> stop.
	SetScannerOn(false);
}

void AScanningTool::DeactivateTool()
{
	// Holstering must stop the scan and clear the material, even if the trigger is still held (Tick is
	// disabled on deactivate, so ScanSurface won't run to reset it on its own).
	SetScannerOn(false);
	if (SurfaceScannerCollection)
	{
		ResetScanningArea();
	}

	Super::DeactivateTool();
}

void AScanningTool::SeeThroughSurface(FVector& OutHitLocation)
{
	if (!bIsScannerOn)
	{
		ResetScanningArea();
		return;
	}

	const FVector BeginTrace = Muzzle->GetComponentLocation();
	const FVector Direction = Muzzle->GetForwardVector();
	const FVector EndTrace = BeginTrace + Direction * ScanningDistance;

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ScanTrace), /*bTraceComplex=*/true, this);

	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, BeginTrace, EndTrace, ECollisionChannel::ECC_WorldStatic, Params);
	if (bHit)
	{
		// UE_LOG(LogTemp, Warning, TEXT("ScanningTool: Found Hit."));
		OutHitLocation = Hit.Location;
		const FLinearColor ScanAreaCenter(OutHitLocation);
		UKismetMaterialLibrary::SetScalarParameterValue(GetWorld(), SurfaceScannerCollection, FName("ScannerFlag"), 1);
		UKismetMaterialLibrary::SetVectorParameterValue(GetWorld(), SurfaceScannerCollection, FName("ScanAreaCenter"), ScanAreaCenter);
	}
	else
	{
		// UE_LOG(LogTemp, Error, TEXT("ScanningTool: Found no Hit."));
		ResetScanningArea();
	}
}

void AScanningTool::DiscoverResource(FVector& HitLocation)
{
	// Client-local detection + event-gated reporting: the subsystem detects newly-covered vein
	// samples against the local samples and RPCs each flip to the server. No per-tick stream.
	if (VeinSubsystem)
	{
		VeinSubsystem->ClientReportScan(HitLocation, ScanningRadius);
	}
}

void AScanningTool::MineResource(FVector& HitLocation, float DeltaSeconds)
{
	// Client-local mining prediction (smooth thinning) + one report per sample completed.
	if (VeinSubsystem)
	{
		VeinSubsystem->ClientReportMining(HitLocation, ScanningRadius, DeltaSeconds, MiningRatePerSecond);
	}
}

void AScanningTool::ResetScanningArea()
{
	const FLinearColor NoScan(0, 0, 0);
	UKismetMaterialLibrary::SetVectorParameterValue(GetWorld(), SurfaceScannerCollection, FName("ScanAreaCenter"), NoScan);
	UKismetMaterialLibrary::SetScalarParameterValue(GetWorld(), SurfaceScannerCollection, FName("ScannerFlag"), 0);
}

void AScanningTool::SetScannerMode(EScanMode ScanMode)
{
	CurrentScanMode = ScanMode;
}
