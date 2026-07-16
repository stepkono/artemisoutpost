// Fill out your copyright notice in the Description page of Project Settings.


#include "MoonTexturer.h"

#include "Cesium3DTileset.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "MaterialTypes.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"

UMoonTexturer::UMoonTexturer()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMoonTexturer::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Warning, TEXT("[MoonTexturer] BeginPlay on Owner=%s, NetMode=%d"),
		GetOwner() ? *GetOwner()->GetName() : TEXT("<none>"), (int32)GetNetMode());

	// Rendering-only: nothing to do on a headless server.
	if (GetNetMode() == NM_DedicatedServer)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MoonTexturer] Dedicated server -> skipping rendering setup."));
		return;
	}

	InitializeRendering();
}

void UMoonTexturer::InitializeRendering()
{
	if (bInitialized)
	{
		return;
	}

	if (!CesiumMaterial)
	{
		UE_LOG(LogTemp, Error, TEXT("[MoonTexturer] CesiumMaterial not set on component. Fog will not render."));
		return;
	}

	ACesium3DTileset* Tileset = Cast<ACesium3DTileset>(GetOwner());
	if (!Tileset)
	{
		UE_LOG(LogTemp, Error, TEXT("[MoonTexturer] Owner '%s' is not a Cesium3DTileset. Aborting."),
			GetOwner() ? *GetOwner()->GetName() : TEXT("<none>"));
		return;
	}

	// 32-bit float data texture (RGB = scan world position, A = radius). One row, one texel per scan.
	FogOfWarTexture = UTexture2D::CreateTransient(Capacity, 1, PF_A32B32G32R32F);
	FogOfWarTexture->Filter   = TextureFilter::TF_Nearest;   // exact texels, no blending
	FogOfWarTexture->SRGB     = false;                       // raw data, not color
	FogOfWarTexture->AddressX = TextureAddress::TA_Clamp;
	FogOfWarTexture->AddressY = TextureAddress::TA_Clamp;
	FogOfWarTexture->UpdateResource();

	// Dynamic instance so we can bind the runtime texture; set the parameter once.
	// ScannedAreaData lives inside a material layer, so it must be addressed by layer index,
	// not by plain name (a plain-name set targets the global scope and silently misses it).
	FogMID = UMaterialInstanceDynamic::Create(CesiumMaterial, this);
	const FMaterialParameterInfo ScanDataInfo(TEXT("ScannedAreaData"), EMaterialParameterAssociation::LayerParameter, FogLayerIndex);
	FogMID->SetTextureParameterValueByInfo(ScanDataInfo, FogOfWarTexture);

	Tileset->SetMaterial(FogMID);
	Tileset->RefreshTileset();

	bInitialized = true;

	UE_LOG(LogTemp, Warning, TEXT("[MoonTexturer] Material SET on tileset '%s' (FogLayerIndex=%d, Capacity=%d, MID=%s)."),
		*Tileset->GetName(), FogLayerIndex, Capacity, *FogMID->GetName());
}

void UMoonTexturer::ApplyScanData(const TArray<FLinearColor>& ScanPixels)
{
	if (!bInitialized)
	{
		InitializeRendering();
	}
	if (!FogOfWarTexture || !FogMID)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MoonTexturer] ApplyScanData skipped: not initialized (tex=%s mid=%s)."),
			FogOfWarTexture ? TEXT("OK") : TEXT("NULL"), FogMID ? TEXT("OK") : TEXT("NULL"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[MoonTexturer] ApplyScanData: %d scan pixels."), ScanPixels.Num());

	const int32 Width = FogOfWarTexture->GetSizeX();
	const int32 Bpp   = sizeof(FLinearColor);                 // 16 bytes: 4x float32
	const int32 Count = FMath::Min(ScanPixels.Num(), Width);

	// Heap-allocate the source buffer + region: the render thread reads them LATER,
	// so they must outlive this call. The cleanup lambda frees them after upload.
	uint8* SrcData = new uint8[Width * Bpp];
	FMemory::Memzero(SrcData, Width * Bpp);                    // unused slots -> 0
	FMemory::Memcpy(SrcData, ScanPixels.GetData(), Count * Bpp);

	FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, Width, 1);

	FogOfWarTexture->UpdateTextureRegions(
		0, 1, Region, Width * Bpp, Bpp, SrcData,
		[](uint8* Data, const FUpdateTextureRegion2D* Regions)
		{
			delete[] Data;
			delete Regions;
		});

	// NumScans must reach EVERY Cesium per-tile MID. Setting it on the parent FogMID does NOT propagate
	// to already-created tile MIDs (they keep the value cached at creation -> stale on streamed tiles),
	// which shows up as a patchy, camera-distance-dependent disc. An MPC is global and every tile reads it live.
	if (ScanParameterCollection)
	{
		if (UMaterialParameterCollectionInstance* MPCI = GetWorld()->GetParameterCollectionInstance(ScanParameterCollection))
		{
			MPCI->SetScalarParameterValue(TEXT("NumScans"), static_cast<float>(Count));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[MoonTexturer] ScanParameterCollection not set -> NumScans never reaches the shader."));
	}
}
