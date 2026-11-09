// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PixelRevealSet.generated.h"

class UMaterialInterface;

/**
 * Maps each opaque material to its masked "_Reveal" twin (UseReveal = true, Blend Mode override = Masked).
 * One asset for the whole project. Filled by Content/Python/create_reveal_twins.py.
 * Hard references, so the twins get cooked.
 */
UCLASS(BlueprintType)
class ARTEMISOUTPOST_API UPixelRevealSet : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pixel Reveal")
	TMap<TObjectPtr<UMaterialInterface>, TObjectPtr<UMaterialInterface>> Twins;

	/** Returns the reveal twin of Original, or nullptr if the material has none. */
	UMaterialInterface* FindTwin(UMaterialInterface* Original) const;
};
