// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArtemisOutpost/GameData/ServerGameMode.h"
#include "ArtemisOutpost/MiniGames/General/MinigameTypes.h"
#include "Components/ActorComponent.h"
#include "ConnectionComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSlotsChangedBP);

// Fired server-side when a participant is granted / releases a slot. The minigame logic
// subscribes to react (start, assign axes, abort).
DECLARE_MULTICAST_DELEGATE_OneParam(FOnParticipant, const FString& /*UPID*/);

// Asked by the connection BEFORE a slot is granted, so game-specific preconditions (cost,
// buildability, proximity) stay in the logic layer while slot rules stay here. Unbound = allow.
DECLARE_DELEGATE_RetVal_TwoParams(bool, FCanJoinPredicate, const FString& /*UPID*/, FText& /*OutReason*/);

// Owns the connection between players and a minigame: the VR manipulation slots and the
// join/leave book-keeping — nothing about how the task plays. Keyed on the persistent UPID so
// occupancy is reconnect-stable and attributable for logging (§7). PC/AR are not slots; they
// view the replicated state only.
UCLASS(ClassGroup = (Minigame), meta = (BlueprintSpawnableComponent))
class ARTEMISOUTPOST_API UConnectionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UConnectionComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Server-authoritative connection entry points (no-ops off the authority).
	void ServerRequestJoin(const FString& UPID);
	void ServerRequestLeave(const FString& UPID);

	UFUNCTION(BlueprintPure, Category = "Connection")
	int32 GetMaxSlots() const;

	UFUNCTION(BlueprintPure, Category = "Connection")
	int32 GetFreeSlotCount() const;

	UFUNCTION(BlueprintPure, Category = "Connection")
	int32 GetParticipantCount() const;

	UFUNCTION(BlueprintPure, Category = "Connection")
	bool IsParticipant(const FString& UPID) const;

	// Occupied UPIDs in join order (index 0 = first joiner). Used to partition axes.
	TArray<FString> GetParticipantUPIDs() const;

	int32 GetSlotIndexFor(const FString& UPID) const;

	// Bound by the minigame logic to veto joins on game-specific preconditions.
	FCanJoinPredicate CanJoinPredicate;

	FOnParticipant OnParticipantJoined;
	FOnParticipant OnParticipantLeft;

	UPROPERTY(BlueprintAssignable, Category = "Connection")
	FOnSlotsChangedBP OnSlotsChanged;

protected:
	virtual void BeginPlay() override;
	
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Connection")
	int32 MaxSlots = 1;

	// One entry per occupied slot; array position IS the slot index.
	UPROPERTY(ReplicatedUsing = OnRep_ActiveSlots, BlueprintReadOnly, Category = "Connection")
	TArray<FConnectionSlot> ActiveSlots;

	UFUNCTION()
	void OnRep_ActiveSlots();
	
private: 
	AServerGameMode* GameMode; 
};
