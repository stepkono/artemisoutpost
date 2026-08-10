// Fill out your copyright notice in the Description page of Project Settings.

#include "ArtemisOutpost/Player/MiniGameInteraction/MinigameClientComponent.h"
#include "ArtemisOutpost/Player/PawnController.h"
#include "ArtemisOutpost/Minigame/MinigameActor.h"
#include "ArtemisOutpost/Minigame/MinigameLogicComponent.h"
#include "ArtemisOutpost/Connection/ConnectionComponent.h"

UMinigameClientComponent::UMinigameClientComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	// Must be replicated for the Server RPCs to route through the owning controller.
	SetIsReplicatedByDefault(true);
}

FString UMinigameClientComponent::GetOwnerUPID() const
{
	const APawnController* PC = Cast<APawnController>(GetOwner());
	return PC ? PC->GetPlayerUPID() : FString();
}

void UMinigameClientComponent::ServerRequestEnter_Implementation(AMinigameActor* Target)
{
	if (Target)
	{
		if (UConnectionComponent* Connection = Target->GetConnectionComponent())
		{
			Connection->ServerRequestJoin(GetOwnerUPID());
		}
	}
}

void UMinigameClientComponent::ServerRequestLeave_Implementation(AMinigameActor* Target)
{
	if (Target)
	{
		if (UConnectionComponent* Connection = Target->GetConnectionComponent())
		{
			Connection->ServerRequestLeave(GetOwnerUPID());
		}
	}
}

void UMinigameClientComponent::ServerSubmitInput_Implementation(AMinigameActor* Target, FMinigameInput Input)
{
	if (Target)
	{
		if (UMinigameLogicComponent* Logic = Target->GetLogicComponent())
		{
			Logic->ServerHandleInput(GetOwnerUPID(), Input);
		}
	}
}
