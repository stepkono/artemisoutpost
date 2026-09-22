// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArtemisOutpost/StudyData/Providers/DataProviderSubsystemBase.h"
#include "ArtemisOutpost/StudyData/Types/ProviderDataBase.h"
#include "ArtemisOutpost/Player/PlayerCues/PlayerCueTypes.h"
#include "PlayerActionProvider.generated.h"

// Network seam for player-scoped events. The DataAggregator subscribes here. Server-side.
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPlayerActionEvent, UProviderDataBase&, const EGameEventType);

/**
 * Server-side provider for player-scoped study events. Entry points are called from the
 * authoritative APawnController (HMD state) and from AArtemisPlayerState (awareness cues); this
 * builds the provider payload and re-broadcasts it on the aggregator seam.
 */
UCLASS()
class ARTEMISOUTPOST_API UPlayerActionProvider : public UDataProviderSubsystemBase
{
	GENERATED_BODY()

public:
	// SERVER: called from APawnController::ServerReportHmdState_Implementation. Builds the payload
	// and broadcasts the matching HMDDonned/HMDDoffed event.
	void ServerReportHmdState(const FString& InUPID, bool bWorn);

	// SERVER: called from the AArtemisPlayerState mutators. The full cue state rides along so every
	// event is self-describing on the web side (no state to reconstruct there).
	void ServerReportCueEvent(const FString& InUPID, const FPlayerCueState& State, EGameEventType Event);

	// Aggregator subscribes here.
	FOnPlayerActionEvent OnPlayerActionEvent;
};
