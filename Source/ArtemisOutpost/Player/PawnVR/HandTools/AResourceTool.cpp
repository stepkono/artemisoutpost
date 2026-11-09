// Fill out your copyright notice in the Description page of Project Settings.


#include "AResourceTool.h"

#include "Kismet/KismetMaterialLibrary.h"
#include "GameFramework/Pawn.h"


// Sets default values
AResourceTool::AResourceTool()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void AResourceTool::BeginPlay()
{
	Super::BeginPlay();
	
	if (UMoonResourcesManager* RVS = GetWorld()->GetSubsystem<UMoonResourcesManager>())
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
void AResourceTool::Tick(float DeltaTime)
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

void AResourceTool::SetScannerOn(bool bScannerOn)
{
	bIsScannerOn = bScannerOn;
}

void AResourceTool::BeginScan(EScanMode Mode)
{
	UE_LOG(LogTemp, Log, TEXT("[ResourceTool] BeginScan: Mode=%s, tool %s."),
		*UEnum::GetValueAsString(Mode), bToolActive ? TEXT("already drawn") : TEXT("drawing now"));

	CurrentScanMode = Mode;
	if (!bToolActive)
	{
		ActivateTool(); // reports the activity itself
	}
	else
	{
		// Already drawn, only the mode changed: the HUD text must follow.
		ReportToolActivity(true);
	}
}

EToolActivity AResourceTool::GetToolActivity() const
{
	switch (CurrentScanMode)
	{
	case EScanMode::Area:    return EToolActivity::ScanArea;
	case EScanMode::Mining:  return EToolActivity::CollectResource;
	case EScanMode::Surface:
	default:                 return EToolActivity::ScanSurface;
	}
}

void AResourceTool::ExecuteAction()
{
	// Trigger held -> scan.
	SetScannerOn(true);
}

void AResourceTool::EndAction()
{
	// Trigger released -> stop.
	SetScannerOn(false);
}

void AResourceTool::DeactivateTool()
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

void AResourceTool::SeeThroughSurface(FVector& OutHitLocation)
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

	// The holder's capsule (object type Pawn) blocks ECC_WorldStatic. Without this the aim line hits the
	// own body at hip height whenever it crosses the capsule, the hit is too far from the floor pixels
	// and the see-through hole disappears in a cone around the player. The actors attached to the pawn
	// (the other hand tool, the placement visualizer) are ignored for the same reason.
	if (APawn* Holder = GetOwningPawn())
	{
		Params.AddIgnoredActor(Holder);
		TArray<AActor*> AttachedToHolder;
		Holder->GetAttachedActors(AttachedToHolder, /*bResetArray=*/true, /*bRecursivelyIncludeAttachedActors=*/true);
		Params.AddIgnoredActors(AttachedToHolder);
	}

	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, BeginTrace, EndTrace, ECollisionChannel::ECC_WorldStatic, Params);

	// Verification aid: log the blocking actor only when it changes (never the VR char after the fix).
	if (Hit.GetActor() != LastScanHitActor.Get())
	{
		LastScanHitActor = Hit.GetActor();
		UE_LOG(LogTemp, Verbose, TEXT("[ResourceTool] SeeThroughSurface: trace now hits %s."), *GetNameSafe(Hit.GetActor()));
	}

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

void AResourceTool::DiscoverResource(FVector& HitLocation)
{
	// Client-local detection + event-gated reporting: the subsystem detects newly-covered vein
	// samples against the local samples and RPCs each flip to the server. No per-tick stream.
	if (VeinSubsystem)
	{
		VeinSubsystem->ClientReportScan(HitLocation, ScanningRadius);
	}
}

void AResourceTool::MineResource(FVector& HitLocation, float DeltaSeconds)
{
	// Client-local mining prediction (smooth thinning) + one report per sample completed.
	// Uses a wider radius than discovery so mining is forgiving to aim.
	if (VeinSubsystem)
	{
		VeinSubsystem->ClientReportMining(HitLocation, ScanningRadius * MiningRadiusFactor, DeltaSeconds, MiningRatePerSecond);
	}
}

void AResourceTool::ResetScanningArea()
{
	const FLinearColor NoScan(0, 0, 0);
	UKismetMaterialLibrary::SetVectorParameterValue(GetWorld(), SurfaceScannerCollection, FName("ScanAreaCenter"), NoScan);
	UKismetMaterialLibrary::SetScalarParameterValue(GetWorld(), SurfaceScannerCollection, FName("ScannerFlag"), 0);
}

void AResourceTool::SetScannerMode(EScanMode ScanMode)
{
	CurrentScanMode = ScanMode;
}
