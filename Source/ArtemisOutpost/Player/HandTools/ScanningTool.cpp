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
	
	Muzzle = Cast<USceneComponent>(FindComponentByTag<UActorComponent>(FName("ScanningTool")));
	if (!Muzzle)
	{
		UE_LOG(LogTemp, Error, TEXT("ScanningTool: Failed to get Muzzle."));
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
	
	ScanSurface();
}

void AScanningTool::SetScannerOn(bool bScannerOn)
{
	bIsScannerOn = bScannerOn;
}

void AScanningTool::ScanSurface()
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
		const FVector Location = Hit.Location; 
		const FLinearColor ScanAreaCenter(Location);
		UKismetMaterialLibrary::SetScalarParameterValue(GetWorld(), SurfaceScannerCollection, FName("ScannerFlag"), 1);
		UKismetMaterialLibrary::SetVectorParameterValue(GetWorld(), SurfaceScannerCollection, FName("ScanAreaCenter"), ScanAreaCenter);
	}
	else
	{
		ResetScanningArea(); 
	}
}

void AScanningTool::ResetScanningArea()
{
	const FLinearColor NoScan(0, 0, 0);
	UKismetMaterialLibrary::SetVectorParameterValue(GetWorld(), SurfaceScannerCollection, FName("ScanAreaCenter"), NoScan);
	UKismetMaterialLibrary::SetScalarParameterValue(GetWorld(), SurfaceScannerCollection, FName("ScannerFlag"), 0);
}
