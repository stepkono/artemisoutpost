// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Cesium3DTileset.h"
#include "CesiumGeoreference.h"
#include "ArtemisOutpost/GameData/ArtemisGameState.h"
#include "GameFramework/Pawn.h"
#include "PawnAR.generated.h"

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
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

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
