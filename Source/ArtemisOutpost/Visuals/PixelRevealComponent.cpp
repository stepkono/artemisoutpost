// Fill out your copyright notice in the Description page of Project Settings.

#include "PixelRevealComponent.h"

#include "PixelRevealSet.h"
#include "Components/MeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogPixelReveal, Log, All);

UPixelRevealComponent::UPixelRevealComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UPixelRevealComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!bPlayOnBeginPlay)
	{
		return;
	}

	if (bSkipLevelPlacedActors && GetOwner() && GetOwner()->IsNetStartupActor())
	{
		return;
	}

	PlayReveal();
}

void UPixelRevealComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RestoreTimer);
	}

	Super::EndPlay(EndPlayReason);
}

void UPixelRevealComponent::PlayReveal()
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner)
	{
		return;
	}

	if (!RevealSet)
	{
		UE_LOG(LogPixelReveal, Warning, TEXT("%s: no RevealSet assigned, skipping reveal."), *Owner->GetName());
		return;
	}

	// Restart cleanly if a reveal is still running.
	RestoreMaterials();

	// Same clock as the material Time node (game time, respects pause).
	const float StartTime = static_cast<float>(World->GetTimeSeconds());

	TInlineComponentArray<UMeshComponent*> Meshes(Owner);
	for (UMeshComponent* Mesh : Meshes)
	{
		bool bSwappedAny = false;

		for (int32 SlotIndex = 0; SlotIndex < Mesh->GetNumMaterials(); ++SlotIndex)
		{
			UMaterialInterface* Original = Mesh->GetMaterial(SlotIndex);
			UMaterialInterface* Twin = RevealSet->FindTwin(Original);
			if (!Twin)
			{
				continue;
			}

			FPixelRevealSlot& Swapped = SwappedSlots.AddDefaulted_GetRef();
			Swapped.Mesh = Mesh;
			Swapped.SlotIndex = SlotIndex;
			Swapped.Original = Original;

			Mesh->SetMaterial(SlotIndex, Twin);
			bSwappedAny = true;
		}

		if (bSwappedAny)
		{
			Mesh->SetCustomPrimitiveDataFloat(CustomDataIndex, StartTime);
			Mesh->SetCustomPrimitiveDataFloat(CustomDataIndex + 1, Duration);
		}
	}

	if (SwappedSlots.Num() == 0)
	{
		UE_LOG(LogPixelReveal, Verbose, TEXT("%s: no material has a reveal twin."), *Owner->GetName());
		return;
	}

	World->GetTimerManager().SetTimer(RestoreTimer, this, &UPixelRevealComponent::RestoreMaterials,
		Duration + RestoreMargin, false);
}

void UPixelRevealComponent::RestoreMaterials()
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RestoreTimer);
	}

	for (const FPixelRevealSlot& Swapped : SwappedSlots)
	{
		if (UMeshComponent* Mesh = Swapped.Mesh.Get())
		{
			Mesh->SetMaterial(Swapped.SlotIndex, Swapped.Original);
		}
	}

	SwappedSlots.Reset();
}
