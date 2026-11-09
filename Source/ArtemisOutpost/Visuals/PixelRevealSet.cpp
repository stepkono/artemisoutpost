// Fill out your copyright notice in the Description page of Project Settings.

#include "PixelRevealSet.h"

#include "Materials/MaterialInterface.h"

UMaterialInterface* UPixelRevealSet::FindTwin(UMaterialInterface* Original) const
{
	if (!Original)
	{
		return nullptr;
	}

	const TObjectPtr<UMaterialInterface>* Found = Twins.Find(Original);
	return Found ? Found->Get() : nullptr;
}
