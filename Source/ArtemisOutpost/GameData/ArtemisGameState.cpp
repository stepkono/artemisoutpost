// Fill out your copyright notice in the Description page of Project Settings.


#include "ArtemisGameState.h"
#include "ArtemisPlayerState.h"
#include "ArtemisOutpost/Player/PawnVR/ACharVR.h"
#include "ArtemisOutpost/Player/PawnAR/PawnAR.h"
#include "ArtemisOutpost/Player/Rover/AMasterRover.h"
#include "ArtemisOutpost/Moon/MoonScannedArea/MoonScannedAreaManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"

static const FString AnchorSaveSlot = TEXT("ArtemisAnchorSave");

AArtemisGameState::AArtemisGameState()
{
	MoonDataManager = CreateDefaultSubobject<UMoonScannedAreaManager>(TEXT("MoonDataManager"));
}

AArtemisGameState* AArtemisGameState::GetArtemisGameState(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetGameState<AArtemisGameState>() : nullptr;
}

void AArtemisGameState::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority()) return; // server only — clients receive anchors via replication

	FOrderedAnchors LoadedAnchors;
	if (!GetAnchorsFromCurrentSession(LoadedAnchors))
	{
		UE_LOG(LogTemp, Log, TEXT("AArtemisGameState: No saved anchors found — waiting for host-quest to upload."));
	}
}

void AArtemisGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(AArtemisGameState, RawAnchors);
	DOREPLIFETIME(AArtemisGameState, SharingGroupUUID);
	DOREPLIFETIME(AArtemisGameState, MapBaseCoordinates);
	DOREPLIFETIME(AArtemisGameState, PlayersRoster);
}

void AArtemisGameState::WritePlayer(const FArtemisPlayer& Player)
{
	if (!HasAuthority())
	{
		return;
	}

	if (FArtemisPlayer* Entry = PlayersRoster.FindByPredicate([&Player](const FArtemisPlayer& P) { return P.UPID == Player.UPID; }))
	{
		*Entry = Player;
	}
	else
	{
		PlayersRoster.Add(Player);
	}
}

bool AArtemisGameState::FindRecordForActor(const AActor* Actor, FArtemisPlayer& OutPlayer) const
{
	if (!Actor)
	{
		return false;
	}

	for (const FArtemisPlayer& Player : PlayersRoster)
	{
		// The AR pawn is per connection and not stored in the roster, it is only known through the live PlayerState.
		const AActor* ARPawn = Player.PlayerState ? Player.PlayerState->GetARPawn() : nullptr;
		if ((Player.VRChar && Player.VRChar == Actor) || (Player.MasterRover && Player.MasterRover == Actor) || (ARPawn && ARPawn == Actor))
		{
			OutPlayer = Player;
			return true;
		}
	}
	return false;
}

void AArtemisGameState::OnRep_RawAnchors()
{
	// [AnchorRepl] Client received replicated anchors. Log the UUIDs and whether the session
	// group UUID is already present — if the group is still invalid here, RequestSharedAnchors
	// will abort (replication-ordering bug).
	UE_LOG(LogTemp, Warning, TEXT("[AnchorRepl] OnRep_RawAnchors on client. A=%s B=%s C=%s D=%s | GroupUUID valid=%d (%s)"),
		*RawAnchors.AAnchorUUID.ToString(), *RawAnchors.BAnchorUUID.ToString(),
		*RawAnchors.CAnchorUUID.ToString(), *RawAnchors.DAnchorUUID.ToString(),
		SharingGroupUUID.IsValidUUID(), *SharingGroupUUID.ToString());

	OnRawAnchorsUpdated.Broadcast(RawAnchors);
}

void AArtemisGameState::OnRep_SharingGroupUUID()
{
	// [AnchorRepl] Observe when the group UUID arrives relative to the anchors. If this fires
	// AFTER OnRep_RawAnchors, the first RequestSharedAnchors ran without a valid group.
	UE_LOG(LogTemp, Warning, TEXT("[AnchorRepl] OnRep_SharingGroupUUID on client. GroupUUID valid=%d (%s)"),
		SharingGroupUUID.IsValidUUID(), *SharingGroupUUID.ToString());
}

void AArtemisGameState::OnRep_MapBaseCoordinates()
{
	OnMapCoordinatesReceived.Broadcast(MapBaseCoordinates);
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

void AArtemisGameState::WriteGroupUUID(const FOculusXRUUID& GroupUUID)
{
	UE_LOG(LogTemp, Display, TEXT("AArtemisGameState: Writing session group UUID: %s"), *GroupUUID.ToString());
	SharingGroupUUID = GroupUUID;
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

bool AArtemisGameState::GetAnchorsFromCurrentSession(FOrderedAnchors& OutAnchors) const
{
	if (!UGameplayStatics::DoesSaveGameExist(AnchorSaveSlot, 0))
	{
		return false;
	}

	UAnchorSaveGame* SaveGame = Cast<UAnchorSaveGame>(UGameplayStatics::LoadGameFromSlot(AnchorSaveSlot, 0));

	if (!SaveGame)
	{
		UE_LOG(LogTemp, Warning, TEXT("AArtemisGameState: Save slot exists but failed to load."));
		return false;
	}
	
	if (!(SaveGame->ServerAnchors.AAnchorUUID.IsValidUUID() && 
		SaveGame->ServerAnchors.BAnchorUUID.IsValidUUID()   && 
		SaveGame->ServerAnchors.CAnchorUUID.IsValidUUID()   && 
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
	UE_LOG(LogTemp, Display, TEXT("ArtemisGameState: Writing new controls..."))
	ControlCommand = Command;
	OnControlCommandReceived.Broadcast(ControlCommand);
}
