// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PuppetRover.generated.h"

UCLASS()
class ARTEMISOUTPOST_API APuppetRover : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	APuppetRover();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};
