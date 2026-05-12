// Fill out your copyright notice in the Description page of Project Settings.


#include "ArtemisGameState.h"

// In .cpp
#include "Net/UnrealNetwork.h"

void AArtemisGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(AArtemisGameState, RawAnchors);
	DOREPLIFETIME(AArtemisGameState, MapBaseCoordinates);
	DOREPLIFETIME(AArtemisGameState, ControlCommand);
}

void AArtemisGameState::OnRep_RawAnchors()
{
	OnRawAnchorsUpdated.Broadcast(RawAnchors); 
}

void AArtemisGameState::OnRep_MapBaseCoordinates()
{
	OnMapCoordinatesReceived.Broadcast(MapBaseCoordinates);
}

void AArtemisGameState::OnRep_ControlCommand()
{
	OnControlCommandReceived.Broadcast(ControlCommand);
}

void AArtemisGameState::WriteRawAnchors(const FCustomAnchors& Anchors)
{
	RawAnchors = Anchors;
}

void AArtemisGameState::WriteMapCoordinates(const FMapBaseCoordinates& MapCoordinates)
{
	MapBaseCoordinates = MapCoordinates;
}

void AArtemisGameState::WriteControlCommand(const FControlCommand& Command)
{
	ControlCommand = Command;
}
