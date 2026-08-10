// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArtemisOutpost/Connection/Interactable.h"
#include "ArtemisOutpost/Minigame/MinigameTypes.h"
#include "MinigameActor.generated.h"

class UConnectionComponent;
class UMinigameLogicComponent;
class APawnController;

// Common base for every in-world minigame object (Signal Tower, Habitat, ...). Owns the
// connection component and exposes a Blueprint facade so the world-space UMG widget just
// "calls a method on the tower". The facade routes through the local controller's transport
// (see UMinigameClientComponent) because this actor is server-owned and cannot take client
// RPCs directly (§9).
UCLASS(Abstract)
class ARTEMISOUTPOST_API AMinigameActor : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AMinigameActor();

	// IInteractable
	virtual UConnectionComponent* GetConnectionComponent() const override;

	UFUNCTION(BlueprintPure, Category = "Minigame")
	UMinigameLogicComponent* GetLogicComponent() const;

	// Client-side facade for the local player's VR widget. No-op without a local controller.
	UFUNCTION(BlueprintCallable, Category = "Minigame")
	void RequestEnter();

	UFUNCTION(BlueprintCallable, Category = "Minigame")
	void RequestLeave();

	UFUNCTION(BlueprintCallable, Category = "Minigame")
	void SubmitInput(FMinigameInput Input);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Minigame")
	UConnectionComponent* Connection;

	// Resolved in BeginPlay from whichever concrete logic component the subclass added.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Minigame")
	UMinigameLogicComponent* Logic;

private:
	// Local player's controller, cached (re-resolved only if it becomes null).
	APawnController* GetLocalController();

	UPROPERTY(Transient)
	APawnController* CachedController = nullptr;
};
