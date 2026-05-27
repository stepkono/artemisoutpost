// Fill out your copyright notice in the Description page of Project Settings.


#include "ArtemisGameState.h"

#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"

static const FString AnchorSaveSlot = TEXT("ArtemisAnchorSave");

void AArtemisGameState::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority()) return; // server only — clients receive anchors via replication

	FOrderedAnchors LoadedAnchors;
	if (!GetAnchorsFromPreviousSessions(LoadedAnchors))
	{
		UE_LOG(LogTemp, Log, TEXT("AArtemisGameState: No saved anchors found — waiting for host-quest to upload."));
	}
}

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

void AArtemisGameState::WriteRawAnchors(const FOrderedAnchors& Anchors)
{
	UE_LOG(LogTemp, Display, TEXT("Written Anchor UUID: %s"), *Anchors.AAnchorUUID.ToString());
	UE_LOG(LogTemp, Display, TEXT("Written Anchor UUID: %s"), *Anchors.BAnchorUUID.ToString());
	UE_LOG(LogTemp, Display, TEXT("Written Anchor UUID: %s"), *Anchors.CAnchorUUID.ToString());
	UE_LOG(LogTemp, Display, TEXT("Written Anchor UUID: %s"), *Anchors.DAnchorUUID.ToString());
	
	RawAnchors = Anchors;
	SaveAnchorsToDisk();
}

void AArtemisGameState::SaveAnchorsToDisk() const
{
	UAnchorSaveGame* SaveGame = Cast<UAnchorSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UAnchorSaveGame::StaticClass())
	);

	SaveGame->ServerAnchors = RawAnchors;
	UGameplayStatics::SaveGameToSlot(SaveGame, AnchorSaveSlot, 0);

	UE_LOG(LogTemp, Log, TEXT("AArtemisGameState: Anchor UUIDs saved to disk."));
}

bool AArtemisGameState::GetAnchorsFromPreviousSessions(FOrderedAnchors& OutAnchors) const
{
	if (!UGameplayStatics::DoesSaveGameExist(AnchorSaveSlot, 0))
	{
		return false;
	}

	UAnchorSaveGame* SaveGame = Cast<UAnchorSaveGame>(
		UGameplayStatics::LoadGameFromSlot(AnchorSaveSlot, 0)
	);

	if (!SaveGame)
	{
		UE_LOG(LogTemp, Warning, TEXT("AArtemisGameState: Save slot exists but failed to load."));
		return false;
	}
	
	if (!(SaveGame->ServerAnchors.AAnchorUUID.IsValidUUID() && 
		SaveGame->ServerAnchors.BAnchorUUID.IsValidUUID() && 
		SaveGame->ServerAnchors.CAnchorUUID.IsValidUUID() && 
		SaveGame->ServerAnchors.DAnchorUUID.IsValidUUID()))
	{
		UE_LOG(LogTemp, Warning, TEXT("AArtemisGameState: No Anchors Data was saved to the slot."));
		return false;
	}

	OutAnchors = SaveGame->ServerAnchors;
	return true;
}

void AArtemisGameState::WriteMapCoordinates(const FMapBaseCoordinates& MapCoordinates)
{
	MapBaseCoordinates = MapCoordinates;
}

void AArtemisGameState::WriteControlCommand(const FControlCommand& Command)
{
	ControlCommand = Command;
}
