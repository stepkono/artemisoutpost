// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SplineComponent.h"
#include "ResourceVeinSpline.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ARTEMISOUTPOST_API UResourceVeinSpline : public USplineComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UResourceVeinSpline();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;
};
