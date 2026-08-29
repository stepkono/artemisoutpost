// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArtemisOutpost/MiniGames/General/GameInstance/MinigameActor.h"
#include "SolarPanel.generated.h"

UCLASS()
class ARTEMISOUTPOST_API ASolarPanel : public AMinigameActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ASolarPanel();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual EMiniGameType GetBuildingType() const override { return EMiniGameType::SolarPanel; }

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};
