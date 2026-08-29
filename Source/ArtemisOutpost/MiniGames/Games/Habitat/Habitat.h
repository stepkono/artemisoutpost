// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArtemisOutpost/MiniGames/General/GameInstance/MinigameActor.h"
#include "Habitat.generated.h"

UCLASS()
class ARTEMISOUTPOST_API AHabitat : public AMinigameActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AHabitat();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// Registers as a Habitat carrying FHabitatData so Signal Towers can find and claim it.
	virtual EMiniGameType GetBuildingType() const override { return EMiniGameType::Habitat; }
	virtual FInstancedStruct MakeInitialTypeData() const override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};
