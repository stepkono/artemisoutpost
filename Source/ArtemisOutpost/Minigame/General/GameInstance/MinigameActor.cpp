// Fill out your copyright notice in the Description page of Project Settings.

#include "MinigameActor.h"

#include "ArtemisOutpost/Minigame/General/Logic/MinigameLogicComponent.h"
#include "ArtemisOutpost/Minigame/Connection/ConnectionComponent.h"
#include "ArtemisOutpost/Player/ACharVR.h"
#include "ArtemisOutpost/Player/PawnController.h"
#include "ArtemisOutpost/Player/MiniGameInteraction/MinigamePlayerController.h"
#include "Kismet/GameplayStatics.h"

AMinigameActor::AMinigameActor()
{
	PrimaryActorTick.bCanEverTick = true;

	// Few minigame actors exist and both nearby VR manipulators and the (possibly distant) AR
	// proxy need the state, so keep them always relevant rather than distance-culled.
	bReplicates = true;
	bAlwaysRelevant = true;
	
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneRoot"));
	RootComponent = Root;

	GameConnection = CreateDefaultSubobject<UConnectionComponent>(TEXT("Connection"));
}

void AMinigameActor::BeginPlay()
{
	Super::BeginPlay();

	GameLogic          = FindComponentByClass<UMinigameLogicComponent>();
	ConnectionUIHolder = FindComponentByClass<UMiniGameConnectionUIComponent>();
}

void AMinigameActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	if (ConnectionUIHolder)
	{
		ConnectionUIHolder->SetShowConnectionUI(IsPlayerNear() && CanLocalPlayerConnect());
	}
}

UConnectionComponent* AMinigameActor::GetConnectionComponent() const
{
	return GameConnection;
}

UMinigameLogicComponent* AMinigameActor::GetLogicComponent() const
{
	return GameLogic;
}

APawnController* AMinigameActor::GetLocalController() const
{
	if (!CachedController)
	{
		CachedController = Cast<APawnController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
	}
	return CachedController;
}

FString AMinigameActor::GetLocalPlayerUPID() const
{
	const APawnController* PC = GetLocalController();
	return PC ? PC->GetPlayerUPID() : FString();
}

bool AMinigameActor::IsLocalPlayerParticipant() const
{
	const UConnectionComponent* Conn = GetConnectionComponent();
	return Conn ? Conn->IsParticipant(GetLocalPlayerUPID()) : false;
}

bool AMinigameActor::HasFreeSlot() const
{
	const UConnectionComponent* Conn = GetConnectionComponent();
	return Conn ? Conn->GetFreeSlotCount() > 0 : false;
}

bool AMinigameActor::CanLocalPlayerConnect() const
{
	const bool bFinished = GameLogic && GameLogic->GetState() == EMinigameState::Completed;
	return !bFinished && HasFreeSlot() && !IsLocalPlayerParticipant();
}

bool AMinigameActor::IsPlayerNear()
{
	const APawnController* PC = GetLocalController();
	const ACharVR* Pawn = PC ? PC->GetVRPawn() : nullptr;
	if (!Pawn)
	{
		return false;
	}
	return FVector::Dist(Pawn->GetActorLocation(), GetActorLocation()) < 300.0f;
}