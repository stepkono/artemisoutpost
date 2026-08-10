// Fill out your copyright notice in the Description page of Project Settings.

#include "ArtemisOutpost/Minigame/MinigameActor.h"
#include "ArtemisOutpost/Connection/ConnectionComponent.h"
#include "ArtemisOutpost/Minigame/MinigameLogicComponent.h"
#include "ArtemisOutpost/Player/PawnController.h"
#include "ArtemisOutpost/Player/MiniGameInteraction/MinigameClientComponent.h"
#include "Kismet/GameplayStatics.h"

AMinigameActor::AMinigameActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// Few minigame actors exist and both nearby VR manipulators and the (possibly distant) AR
	// proxy need the state, so keep them always relevant rather than distance-culled.
	bReplicates = true;
	bAlwaysRelevant = true;

	Connection = CreateDefaultSubobject<UConnectionComponent>(TEXT("Connection"));
}

void AMinigameActor::BeginPlay()
{
	Super::BeginPlay();

	// The concrete subclass adds the specific logic component; resolve it once.
	Logic = FindComponentByClass<UMinigameLogicComponent>();
}

UConnectionComponent* AMinigameActor::GetConnectionComponent() const
{
	return Connection;
}

UMinigameLogicComponent* AMinigameActor::GetLogicComponent() const
{
	return Logic;
}

APawnController* AMinigameActor::GetLocalController()
{
	if (!CachedController)
	{
		CachedController = Cast<APawnController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
	}
	return CachedController;
}

void AMinigameActor::RequestEnter()
{
	if (APawnController* PC = GetLocalController())
	{
		if (UMinigameClientComponent* Transport = PC->GetMinigameClient())
		{
			Transport->ServerRequestEnter(this);
		}
	}
}

void AMinigameActor::RequestLeave()
{
	if (APawnController* PC = GetLocalController())
	{
		if (UMinigameClientComponent* Transport = PC->GetMinigameClient())
		{
			Transport->ServerRequestLeave(this);
		}
	}
}

void AMinigameActor::SubmitInput(FMinigameInput Input)
{
	if (APawnController* PC = GetLocalController())
	{
		if (UMinigameClientComponent* Transport = PC->GetMinigameClient())
		{
			Transport->ServerSubmitInput(this, Input);
		}
	}
}
