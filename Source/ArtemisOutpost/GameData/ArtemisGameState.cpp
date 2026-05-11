// Fill out your copyright notice in the Description page of Project Settings.


#include "ArtemisGameState.h"

// In .cpp
#include "Net/UnrealNetwork.h"

void AArtemisGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AArtemisGameState, RawAnchors);
}

void AArtemisGameState::OnRep_RawAnchors()
{
	OnRawAnchorsUpdated.Broadcast(RawAnchors); 
}