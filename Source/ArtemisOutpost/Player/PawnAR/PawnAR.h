// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Cesium3DTileset.h"
#include "CesiumGeoreference.h"
#include "ArtemisOutpost/GameData/ArtemisGameState.h"
#include "GameFramework/Pawn.h"
#include "PawnAR.generated.h"

class UControllerRayComponent;
class UPlayerCuesManager;
class UAwarenessHUDComponent;

UCLASS()
class ARTEMISOUTPOST_API APawnAR : public APawn
{
	GENERATED_BODY()

public:
	// Sets default values for this pawn's properties
	APawnAR();

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	virtual void NotifyControllerChanged() override;

	UFUNCTION(BlueprintCallable, Category = "AR Moon")
	ACesium3DTileset* GetARTileset();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// Controller rays for the AR hands (widget click + the shared pointer). The
	// WidgetInteractionComponents it drives must be authored under MotionControllerL/R in BP_ARPawn
	// and tagged Ray_Left / Ray_Right. See UControllerRayComponent.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Controller Rays")
	TObjectPtr<UControllerRayComponent> ControllerRayComponent;

	// Awareness cues producer; this pawn's hits are expressed against the AR table moon.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player Cues")
	TObjectPtr<UPlayerCuesManager> PlayerCuesManager;

	// Hold-to-view HUD overview. Renders into a WidgetComponent tagged Awareness_HUD under Camera.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player Cues")
	TObjectPtr<UAwarenessHUDComponent> AwarenessHUDComponent;

private:
	UFUNCTION(BlueprintCallable)
	bool IsAuthoritativeClient() const;

	UFUNCTION(Server, Reliable)
	void ShareAnchorsWithServer(FOrderedAnchors RawAnchors);

	/** Sends the current session group UUID to the server for replication. Call BEFORE
	 *  ShareAnchorsWithServer so the group UUID is set on the GameState before the anchors. */
	UFUNCTION(Server, Reliable)
	void ShareGroupUUIDWithServer(FOculusXRUUID GroupUUID);

private:
	UPROPERTY()
	UArtemisGameInstance* GI;

	UPROPERTY()
	ACesium3DTileset* ARTileSet;
};
