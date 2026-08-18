// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ArtemisOutpost/Minigame/General/MinigameTypes.h"
#include "MinigamePuppetManager.generated.h"

class AGeoRefsManager;
class AMinigamePuppet;

// Manages the AR puppet for a minigame actor: spawns it at the master's geodetic pose re-anchored on
// the (movable) AR moon, parents it, and forwards the master's replicated data to it. The owning
// AMinigameActor drives it — this component exposes methods, it does not self-trigger.
UCLASS(ClassGroup = (Minigame), meta = (BlueprintSpawnableComponent))
class ARTEMISOUTPOST_API UMinigamePuppetManager : public UActorComponent
{
	GENERATED_BODY()

public:
	UMinigamePuppetManager();

	// Spawns the puppet and attaches it to the AR moon. Called by the owning actor on peers that have
	// a local player (any XR mode); no-op on the dedicated server. Idempotent.
	void CreateARPuppet();

	AMinigamePuppet* GetARPuppet() const { return ARPuppet; }

	// Forward the master's data to the puppet. No-op where no puppet exists (e.g. dedicated server).
	void PushState(EMinigameState NewState);
	void PushAxes(const TArray<FAxisState>& Axes);

protected:
	virtual void BeginPlay() override;

	// The BP puppet to spawn (static-mesh copy). Set per game on the owning actor's BP.
	UPROPERTY(EditDefaultsOnly, Category = "Puppet Manager")
	TSubclassOf<AMinigamePuppet> PuppetClass;

private:
	UPROPERTY()
	AGeoRefsManager* GeoRefsManager;

	UPROPERTY()
	AMinigamePuppet* ARPuppet;
};
