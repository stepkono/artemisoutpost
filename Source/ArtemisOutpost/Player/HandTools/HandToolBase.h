// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HandToolBase.generated.h"

/**
 * Base for the tool held in the player's right hand (Building / Scanning). One instance of each
 * lives permanently on the pawn as a Child Actor Component under the right controller; modes are
 * switched by activating one and deactivating the others — never by spawning/destroying (avoids VR
 * hitches). Client-local: these are visual/aiming tools; the authoritative action (spawn building /
 * add scan) goes through APawnController.
 */
UCLASS(Abstract)
class ARTEMISOUTPOST_API AHandToolBase : public AActor
{
	GENERATED_BODY()

public:
	AHandToolBase();

	// Show + enable this tool (and its per-frame aiming logic).
	UFUNCTION(BlueprintCallable, Category = "Hand Tool")
	virtual void ActivateTool();

	// Hide + disable this tool.
	UFUNCTION(BlueprintCallable, Category = "Hand Tool")
	virtual void DeactivateTool();

	UFUNCTION(BlueprintPure, Category = "Hand Tool")
	bool IsToolActive() const { return bToolActive; }

protected:
	virtual void BeginPlay() override;

	// Visual hooks — implement in the BP child (equip/holster anim, toggle FX, start/stop the trace).
	UFUNCTION(BlueprintImplementableEvent, Category = "Hand Tool")
	void OnToolActivated();

	UFUNCTION(BlueprintImplementableEvent, Category = "Hand Tool")
	void OnToolDeactivated();

	// True only on the client whose pawn holds this tool. The aiming/trace logic must run only there.
	bool IsOwnerLocallyControlled() const;

	bool bToolActive = false;
};
