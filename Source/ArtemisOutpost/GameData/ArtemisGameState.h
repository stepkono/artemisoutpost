// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArtemisGameInstance.h"
#include "ArtemisOutpost/Anchors/AnchorSaveGame.h"
#include "ArtemisOutpost/Miscellaneous/DataTypes.h"
#include "GameFramework/GameState.h"
#include "ArtemisGameState.generated.h"

class UMoonScannedAreaManager;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAnchorsUpdated, const FOrderedAnchors&, RawAnchors); 
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FControlCommandReceived, FControlCommand&, Command);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMapCoordinatesReceived, FMapBaseCoordinates&, MapBaseCoordinates);

UCLASS(Blueprintable)
class ARTEMISOUTPOST_API AArtemisGameState : public AGameStateBase
{
	GENERATED_BODY()
	
public:
	AArtemisGameState();
	virtual void BeginPlay() override;

	/** The world's GameState as AArtemisGameState, so Blueprints need no Cast node. Null before the GameState exists. */
	UFUNCTION(BlueprintPure, Category = "Artemis", meta = (WorldContext = "WorldContextObject"))
	static AArtemisGameState* GetArtemisGameState(const UObject* WorldContextObject);
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void WriteRawAnchors(const FOrderedAnchors& Anchors);

	/** Server: stores the session group UUID and replicates it to clients. */
	UFUNCTION()
	void WriteGroupUUID(const FOculusXRUUID& GroupUUID);

	/** The replicated session group UUID (valid once the authoritative client has shared). */
	FOculusXRUUID GetSharingGroupUUID() const { return SharingGroupUUID; }

	UFUNCTION()
	void WriteMapCoordinates(const FMapBaseCoordinates& MapCoordinates);

	UFUNCTION()
	void WriteControlCommand(const FControlCommand& Command);
	
	UFUNCTION(BlueprintCallable, Category = "Moon Data")
	UMoonScannedAreaManager* GetMoonDataManager() const { return MoonDataManager; }

	/**
	 * Attempts to load anchor UUIDs saved from a previous session.
	 * Returns true and writes to OutAnchors on success.
	 * Only meaningful on the server — clients receive anchors via replication.
	 */
	bool GetAnchorsFromCurrentSession(FOrderedAnchors& OutAnchors) const;

	/** Server: adds the player to PlayersRoster or replaces the entry with the same UPID. */
	void WritePlayer(const FArtemisPlayer& Player);

	/**
	 * The roster entry owning this actor (VRChar, MasterRover, or the AR pawn of a connected player).
	 * Works on every peer and keeps working while the player is disconnected, then OutPlayer.PlayerState
	 * is null. Puppets have to be unwrapped by the caller. Returns false if the actor belongs to no player.
	 */
	UFUNCTION(BlueprintPure, Category = "Players Management")
	bool FindRecordForActor(const AActor* Actor, FArtemisPlayer& OutPlayer) const;

private:
	void SaveAnchorsToDisk() const;

	UFUNCTION()
	void OnRep_RawAnchors();

	UFUNCTION()
	void OnRep_SharingGroupUUID();

	UFUNCTION()
	void OnRep_MapBaseCoordinates();
	
public: 
	FOnAnchorsUpdated OnRawAnchorsUpdated; 
	UPROPERTY(BlueprintAssignable, Category = "Rover Controls")
	FControlCommandReceived OnControlCommandReceived;
	FMapCoordinatesReceived OnMapCoordinatesReceived;

	/**
	 * Every player that joined this session, including disconnected ones. Unlike GameState->PlayerArray
	 * an entry survives a disconnect, so a parked VRChar or rover can still be resolved to its player.
	 * Written only by AServerGameMode through WritePlayer.
	 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Players Management")
	TArray<FArtemisPlayer> PlayersRoster;

private:
	UPROPERTY()
	UMoonScannedAreaManager* MoonDataManager;
	
	UPROPERTY(ReplicatedUsing=OnRep_RawAnchors)
	FOrderedAnchors RawAnchors;

	/** Session group UUID, replicated to clients. OnRep is for diagnostics (and to observe
	 *  replication ordering relative to RawAnchors). Clients read it on demand in RequestSharedAnchors. */
	UPROPERTY(ReplicatedUsing=OnRep_SharingGroupUUID)
	FOculusXRUUID SharingGroupUUID;

	UPROPERTY(ReplicatedUsing=OnRep_MapBaseCoordinates)
	FMapBaseCoordinates MapBaseCoordinates;
	
	UPROPERTY()
	FControlCommand ControlCommand;
};
