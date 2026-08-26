// Fill out your copyright notice in the Description page of Project Settings.

#include "CesiumPlayerCameraFeeder.h"

#include "CesiumCamera.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

// ─────────────────────────────────────────────────────────────────────────────

ACesiumPlayerCameraFeeder::ACesiumPlayerCameraFeeder()
	: Super()
{
	// Tick is already enabled in the base class; this is just an explicit note.
	PrimaryActorTick.bCanEverTick = true;
}

// ─────────────────────────────────────────────────────────────────────────────

void ACesiumPlayerCameraFeeder::BeginPlay()
{
	// Let the base class run first — it handles deprecated _cameras map init.
	Super::BeginPlay();

	// Pre-allocate slot 0 so the first Tick is always an overwrite, not an Add.
	if (AdditionalCameras.IsEmpty())
	{
		AdditionalCameras.AddDefaulted(1);
	}

	UE_LOG(LogTemp, Display,
		TEXT("[CesiumPlayerCameraFeeder] Ready. FOV = %.1f° | TrackedPawn = %s"),
		FieldOfViewDegrees,
		TrackedPawn ? *TrackedPawn->GetName() : TEXT("(auto-resolve)"));
}

// ─────────────────────────────────────────────────────────────────────────────

void ACesiumPlayerCameraFeeder::Tick(float DeltaTime)
{
	// Run base class first: it ticks its own internal logic (currently a no-op
	// beyond Super::Tick, but keeps us forward-compatible with plugin updates).
	Super::Tick(DeltaTime);

	APawn* Pawn = ResolvePawn();
	if (!Pawn)
	{
		// Pawn not yet spawned — leave AdditionalCameras[0] at its default;
		// Cesium filters out cameras with FOV == 0 in GetAllCameras().
		return;
	}

	// GetActorEyesViewPoint is the XR-correct call: it returns the
	// HMD-tracked head position/rotation rather than the actor root pivot.
	FVector  EyeLoc;
	FRotator EyeRot;
	Pawn->GetActorEyesViewPoint(EyeLoc, EyeRot);

	FCesiumCamera Cam;
	Cam.ParameterSource    = ECameraParameterSource::Manual;
	Cam.Location           = EyeLoc;
	Cam.Rotation           = EyeRot;
	Cam.FieldOfViewDegrees = static_cast<double>(FieldOfViewDegrees);
	Cam.ViewportSize       = GetViewportSize();

	// Always overwrite slot 0 — never accumulate stale entries.
	if (AdditionalCameras.IsEmpty())
	{
		AdditionalCameras.Add(Cam);
	}
	else
	{
		AdditionalCameras[0] = Cam;
	}
}

// ─────────────────────────────────────────────────────────────────────────────

APawn* ACesiumPlayerCameraFeeder::ResolvePawn() const
{
	if (IsValid(TrackedPawn))
	{
		return TrackedPawn;
	}

	if (const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		return PC->GetPawn();
	}

	return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────

FVector2D ACesiumPlayerCameraFeeder::GetViewportSize()
{
	if (GEngine && GEngine->GameViewport)
	{
		FVector2D Size;
		GEngine->GameViewport->GetViewportSize(Size);
		if (Size.X > 0.f && Size.Y > 0.f)
		{
			return Size;
		}
	}

	// Fallback for packaging / editor-PIE before the viewport is fully ready.
	return FVector2D(1920.f, 1080.f);
}
