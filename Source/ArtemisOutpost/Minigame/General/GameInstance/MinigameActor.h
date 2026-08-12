// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArtemisOutpost/Minigame/Connection/Interactable.h"
#include "ArtemisOutpost/Minigame/UI/Connection/MiniGameConnectionUIComponent.h"
#include "MinigameActor.generated.h"

class UConnectionComponent;
class UMinigameLogicComponent;
class APawnController;

// Common base for every in-world minigame object (Signal Tower, Habitat, ...). Owns the
// connection component and exposes a Blueprint facade so the world-space UMG widget just
// "calls a method on the tower". The facade routes through the local controller's transport
// (see UMinigamePlayerController) because this actor is server-owned and cannot take client
// RPCs directly (§9).
UCLASS(Abstract)
class ARTEMISOUTPOST_API AMinigameActor : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AMinigameActor();
	virtual void Tick(float DeltaSeconds) override;

	// IInteractable
	virtual UConnectionComponent* GetConnectionComponent() const override;

	UFUNCTION(BlueprintPure, Category = "Minigame")
	UMinigameLogicComponent* GetLogicComponent() const;

	// Connect flow (called from the world-space Connect-Prompt). The play input runs separately
	// through the screen-space View -> UMinigamePlayerController, not through this actor.
	UFUNCTION(BlueprintCallable, Category = "Minigame")
	void RequestEnter();

	UFUNCTION(BlueprintCallable, Category = "Minigame")
	void RequestLeave();

	// --- Connect-Prompt read hooks (local) ---

	// Persistent identity of the local player. Match against an axis OwnerUPID.
	UFUNCTION(BlueprintPure, Category = "Minigame")
	FString GetLocalPlayerUPID() const;

	// True if the local player currently holds a slot (Enter vs. Leave).
	UFUNCTION(BlueprintPure, Category = "Minigame")
	bool IsLocalPlayerParticipant() const;

	UFUNCTION(BlueprintPure, Category = "Minigame")
	bool HasFreeSlot() const;

	// Whether the Connect-Prompt should be offered to the local player: a free slot exists, the
	// task is not finished, and the local player is not already in. (Proximity + facing stay in
	// the prompt BP.)
	UFUNCTION(BlueprintPure, Category = "Minigame")
	bool CanLocalPlayerConnect() const;
	
private: 
	bool IsPlayerNear(); 

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Minigame")
	UConnectionComponent* GameConnection;

	// Resolved in BeginPlay from whichever concrete logic component the subclass added.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Minigame")
	UMinigameLogicComponent* GameLogic;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minigame UI")
	UMiniGameConnectionUIComponent* ConnectionUIHolder; 

private:
	// Local player's controller, cached (re-resolved only if it becomes null).
	APawnController* GetLocalController() const;

	UPROPERTY(Transient)
	mutable APawnController* CachedController = nullptr;
};
