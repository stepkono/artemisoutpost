// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArtemisGameInstance.h"
#include "ArtemisOutpost/Miscellaneous/DataTypes.h"
#include "GameFramework/GameState.h"
#include "ArtemisGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAnchorsUpdated, const FCustomAnchors&, RawAnchors); 
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FControlCommandReceived, FControlCommand&, Command);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMapCoordinatesReceived, FMapBaseCoordinates&, MapBaseCoordinates);

UCLASS()
class ARTEMISOUTPOST_API AArtemisGameState : public AGameStateBase
{
	GENERATED_BODY()
	
public: 
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	UFUNCTION()
	void WriteRawAnchors(const FCustomAnchors& Anchors);
	
	UFUNCTION()
	void WriteMapCoordinates(const FMapBaseCoordinates& MapCoordinates); 
	
	UFUNCTION()
	void WriteControlCommand(const FControlCommand& Command);
	
private: 
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
	FCustomAnchors RawAnchors;
	
	UPROPERTY(ReplicatedUsing=OnRep_MapBaseCoordinates)
	FMapBaseCoordinates MapBaseCoordinates;
	
	UPROPERTY(ReplicatedUsing=OnRep_ControlCommand)
	FControlCommand ControlCommand;
};
