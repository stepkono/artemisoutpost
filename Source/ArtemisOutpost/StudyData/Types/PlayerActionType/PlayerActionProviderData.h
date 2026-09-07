// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArtemisOutpost/StudyData/Types/ProviderDataBase.h"
#include "PlayerActionProviderData.generated.h"

/**
 * UProviderDataBase payload for player-scoped events that go out through the aggregator.
 * For the HMD worn-state events the payload is intentionally minimal: the persistent player
 * identity (UPID) and whether the HMD is currently worn.
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

	virtual TSharedPtr<FJsonObject> BuildJsonFromData(const EGameEventType GameEventType) override;

private:
	TSharedPtr<FJsonObject> SerializeHmdState(const EGameEventType GameEventType);
};
