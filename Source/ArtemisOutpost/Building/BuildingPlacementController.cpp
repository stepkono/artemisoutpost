// Fill out your copyright notice in the Description page of Project Settings.

#include "BuildingPlacementController.h"

#include "ArtemisOutpost/Minigame/General/GameInstance/MinigameActor.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"

#define LOCTEXT_NAMESPACE "Building"

UBuildingPlacementController::UBuildingPlacementController()
{
	PrimaryComponentTick.bCanEverTick = false;
	// Must be replicated so the Server RPC routes through the owning (client-owned) controller.
	SetIsReplicatedByDefault(true);
}

const APawn* UBuildingPlacementController::GetOwningPawn() const
{
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	return PC ? PC->GetPawn() : nullptr;
}

bool UBuildingPlacementController::CanPlaceBuildingAt(EOutpostBuildingType Type, FVector Location, FVector SurfaceNormal, FText& OutReason) const
{
	OutReason = FText::GetEmpty();

	// Slope: compare the surface normal to the reference "up". Near the player the pawn is aligned to
	// the moon surface, so its up vector is a good stand-in for geodetic up (avoids a georeference
	// round-trip and works identically on client and server).
	FVector ReferenceUp = FVector::UpVector;
	if (const APawn* P = GetOwningPawn())
	{
		ReferenceUp = P->GetActorUpVector();
	}

	const float CosAngle = FVector::DotProduct(SurfaceNormal.GetSafeNormal(), ReferenceUp.GetSafeNormal());
	const float SlopeDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(CosAngle, -1.0f, 1.0f)));
	if (SlopeDeg > MaxPlacementSlopeDegrees)
	{
		OutReason = LOCTEXT("TooSteep", "Untergrund zu steil");
		return false;
	}

	// Proximity: reject if too close to an existing building. Iterating AMinigameActor is RHI-safe
	// (no collision query needed) — all buildings derive from AMinigameActor.
	if (const UWorld* World = GetWorld())
	{
		for (TActorIterator<AMinigameActor> It(World); It; ++It)
		{
			const AMinigameActor* Existing = *It;
			if (Existing && FVector::Dist(Existing->GetActorLocation(), Location) < MinBuildingSpacing)
			{
				OutReason = LOCTEXT("TooClose", "Zu nah an einem Gebäude");
				return false;
			}
		}
	}

	return true;
}

void UBuildingPlacementController::ServerPlaceBuilding_Implementation(EOutpostBuildingType Type, FTransform PlacementTransform)
{
	// Authority re-check. The client already validated for feedback, but the server must never trust
	// it. We use the transform's Z axis as the surface normal (the client aligned it to the ground).
	FText Reason;
	if (!CanPlaceBuildingAt(Type, PlacementTransform.GetLocation(), PlacementTransform.GetUnitAxis(EAxis::Z), Reason))
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildingPlacement] ServerPlaceBuilding rejected (%s): %s"),
			*UEnum::GetValueAsString(Type), *Reason.ToString());
		return;
	}

	const TSubclassOf<AMinigameActor>* ClassPtr = BuildingClasses.Find(Type);
	if (!ClassPtr || !*ClassPtr)
	{
		UE_LOG(LogTemp, Error, TEXT("[BuildingPlacement] ServerPlaceBuilding: no class mapped for %s. Fill BuildingClasses."),
			*UEnum::GetValueAsString(Type));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AMinigameActor* NewBuilding = World->SpawnActor<AMinigameActor>(*ClassPtr, PlacementTransform, SpawnParams);
	UE_LOG(LogTemp, Log, TEXT("[BuildingPlacement] Placed building %s -> %s at %s"),
		*UEnum::GetValueAsString(Type), *GetNameSafe(NewBuilding), *PlacementTransform.GetLocation().ToString());
}

#undef LOCTEXT_NAMESPACE
