// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArtemisOutpost/MiniGames/General/MinigameTypes.h"
#include "MinigamePuppet.generated.h"

// Client-local, non-replicated AR visual of a minigame actor: placed at the same geodetic pose on
// the AR moon and parented to it (via UMinigamePuppetManager). It holds NO reference to the master
// and no game logic — the master PUSHES data in through ApplyState/ApplyAxes and the BP child drives
// its static meshes / beams from those events.
UCLASS()
class ARTEMISOUTPOST_API AMinigamePuppet : public AActor
{
	GENERATED_BODY()

public:
	AMinigamePuppet();

	// Pushed by the master (via the puppet manager). Implement in the BP child to drive visuals.
	UFUNCTION(BlueprintImplementableEvent, Category = "Minigame Puppet")
	void ApplyState(EMinigameState NewState);

	UFUNCTION(BlueprintImplementableEvent, Category = "Minigame Puppet")
	void ApplyAxes(const TArray<FAxisState>& Axes);
};
