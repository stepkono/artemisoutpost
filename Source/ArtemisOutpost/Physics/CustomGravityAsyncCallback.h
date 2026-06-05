// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

struct FGravityAttractorData
{
	FVector Location; 
	double MassDotG;
};

struct ARTEMISOUTPOST_API FCustomGravityAsyncInput : public Chaos::FSimCallbackInput
{
	TArray<FGravityAttractorData> GravityAttractorsData;
	
	void Reset()
	{
		GravityAttractorsData.Empty();
	}
};
/**
 * 
 */
class ARTEMISOUTPOST_API FCustomGravityAsyncCallback : public Chaos::TSimCallbackObject<
	FCustomGravityAsyncInput, 
	Chaos::FSimCallbackNoOutput,
	Chaos::ESimCallbackOptions::PreIntegrate | Chaos::ESimCallbackOptions::Presimulate>
{
public:
	FCustomGravityAsyncCallback();
	virtual ~FCustomGravityAsyncCallback() override;
	
	virtual void OnPreSimulate_Internal() override;
	virtual void OnPreIntegrate_Internal() override;

protected:
	static double GravitationalConstant; 
};