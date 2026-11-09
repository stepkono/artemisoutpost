// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PixelRevealComponent.generated.h"

class UMeshComponent;
class UMaterialInterface;
class UPixelRevealSet;

USTRUCT()
struct FPixelRevealSlot
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<UMeshComponent> Mesh;

	UPROPERTY()
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> Original;
};

/**
 * Plays a one-shot, bottom-to-top pixel reveal on all meshes of the owner.
 *
 * Swaps every material that has a twin in RevealSet to its masked "_Reveal" version,
 * writes start time and duration into Custom Primitive Data, and swaps the opaque
 * originals back once the reveal is done. No tick, the shader animates from the Time node.
 *
 * Purely cosmetic and client-local: every machine plays it on its own BeginPlay.
 */
UCLASS(ClassGroup = (Rendering), meta = (BlueprintSpawnableComponent))
class ARTEMISOUTPOST_API UPixelRevealComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPixelRevealComponent();

	/** CPD slot of RevealStartTime. RevealDuration uses the next slot. Must match the material parameters. */
	static constexpr int32 CustomDataIndex = 0;

	/** Starts (or restarts) the reveal. */
	UFUNCTION(BlueprintCallable, Category = "Pixel Reveal")
	void PlayReveal();

	/** Swaps the opaque originals back immediately. Called automatically when the reveal ends. */
	UFUNCTION(BlueprintCallable, Category = "Pixel Reveal")
	void RestoreMaterials();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pixel Reveal")
	TObjectPtr<UPixelRevealSet> RevealSet;

	/** Reveal duration in seconds. Sent to the material through CPD, so this is the only place to set it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pixel Reveal", meta = (ClampMin = "0.05"))
	float Duration = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pixel Reveal")
	bool bPlayOnBeginPlay = true;

	/** Actors placed in the level (loaded with the map) appear instantly. Only runtime spawns reveal. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pixel Reveal")
	bool bSkipLevelPlacedActors = true;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Extra time before swapping back, so the last cells are surely drawn. */
	static constexpr float RestoreMargin = 0.1f;

	UPROPERTY(Transient)
	TArray<FPixelRevealSlot> SwappedSlots;

	FTimerHandle RestoreTimer;
};
