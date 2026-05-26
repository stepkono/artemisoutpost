// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArtemisGameInstance.h"
#include "ArtemisOutpost/Miscellaneous/DataTypes.h"
#include "ArtemisOutpost/Anchors/AnchorSaveGame.h"
#include "GameFramework/GameState.h"
#include "ArtemisGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAnchorsUpdated, const FOrderedAnchors&, RawAnchors); 
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FControlCommandReceived, FControlCommand&, Command);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMapCoordinatesReceived, FMapBaseCoordinates&, MapBaseCoordinates);

UCLASS()
class ARTEMISOUTPOST_API AArtemisGameState : public AGameStateBase
{
	GENERATED_BODY()
	
public:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void WriteRawAnchors(const FOrderedAnchors& Anchors);

	UFUNCTION()
	void WriteMapCoordinates(const FMapBaseCoordinates& MapCoordinates);

	UFUNCTION()
	void WriteControlCommand(const FControlCommand& Command);

	/**
	 * Attempts to load anchor UUIDs saved from a previous session.
	 * Returns true and writes to OutAnchors on success.
	 * Only meaningful on the server — clients receive anchors via replication.
	 */
	bool GetAnchorsFromPreviousSessions(FOrderedAnchors& OutAnchors) const;

private:
	void SaveAnchorsToDisk() const;

	UFUNCTION()
	void OnRep_RawAnchors();
	
	UFUNCTION()
	void OnRep_ControlCommand();
	
	UFUNCTION()
	void OnRep_MapBaseCoordinates();
	
public: 
	FOnAnchorsUpdated OnRawAnchorsUpdated; 
	FControlCommandReceived OnControlCommandReceived;
	FMapCoordinatesReceived OnMapCoordinatesReceived;
	
private: 
	UPROPERTY(ReplicatedUsing=OnRep_RawAnchors)
	FOrderedAnchors RawAnchors;
	
	UPROPERTY(ReplicatedUsing=OnRep_MapBaseCoordinates)
	FMapBaseCoordinates MapBaseCoordinates;
	
	UPROPERTY(ReplicatedUsing=OnRep_ControlCommand)
	FControlCommand ControlCommand;
};
