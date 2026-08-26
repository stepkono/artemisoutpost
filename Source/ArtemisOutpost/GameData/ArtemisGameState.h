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
