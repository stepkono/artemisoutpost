// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArtemisOutpost/StudyData/Types/ProviderDataBase.h"
#include "ArtemisOutpost/Player/PlayerCues/PlayerCueTypes.h"
#include "PlayerActionProviderData.generated.h"

/**
 * UProviderDataBase payload for player-scoped events that go out through the aggregator: the HMD
 * worn-state events and the awareness-cue events (context, activity, pointing, gaze, talk).
 *
 * JSON uses clean keys: "upid", "event", "hmdWorn", "context", "activity", "minigameType", "tool",
 * "talking", "hand", "target": {"kind", "mgid", "minigameType", "upid", "playerNumber", "geo": {"lon",
 * "lat", "height"}}. Every cue event carries the full state so the web side never has to merge deltas.
 */
UCLASS(BlueprintType)
class ARTEMISOUTPOST_API UPlayerActionProviderData : public UProviderDataBase
{
	GENERATED_BODY()

public:
	// Persistent player identity, filled server-side from the authoritative PawnController.
	UPROPERTY(BlueprintReadOnly, Category = "Player Action")
	FString UPID;

	// HMD worn-state at the moment the event fired (true = donned, false = doffed).
	UPROPERTY(BlueprintReadOnly, Category = "Player Action")
	bool bWorn = false;

	// Snapshot of the player's cue state at the moment of the event (cue events only).
	UPROPERTY(BlueprintReadOnly, Category = "Player Action")
	FPlayerCueState CueState;

	virtual TSharedPtr<FJsonObject> BuildJsonFromData(const EGameEventType GameEventType) override;

private:
	TSharedPtr<FJsonObject> SerializeHmdState(const EGameEventType GameEventType) const;
	TSharedPtr<FJsonObject> SerializeCueEvent(const EGameEventType GameEventType) const;

	static TSharedPtr<FJsonObject> SerializeTarget(const FPointingTarget& Target);
	static TSharedPtr<FJsonObject> SerializeGeo(const FVector& LonLatHeight);
};
