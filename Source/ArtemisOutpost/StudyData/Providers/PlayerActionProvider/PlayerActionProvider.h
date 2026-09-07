// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArtemisOutpost/StudyData/Providers/DataProviderSubsystemBase.h"
#include "ArtemisOutpost/StudyData/Types/ProviderDataBase.h"
#include "PlayerActionProvider.generated.h"

// Network seam for player-scoped events. The DataAggregator subscribes here. Server-side.
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPlayerActionEvent, UProviderDataBase&, const EGameEventType);

/**
 * Server-side provider for player-scoped study events. Entry points are called from the
 * authoritative APawnController (which owns the client -> server RPCs); this builds the
 * provider payload and re-broadcasts it on the aggregator seam.
 */
UCLASS()
class ARTEMISOUTPOST_API UPlayerActionProvider : public UDataProviderSubsystemBase
{
	GENERATED_BODY()

public:
	// SERVER: called from APawnController::ServerReportHmdState_Implementation. Builds the payload
	// and broadcasts the matching HMDDonned/HMDDoffed event.
	void ServerReportHmdState(const FString& InUPID, bool bWorn);

	// Aggregator subscribes here.
	FOnPlayerActionEvent OnPlayerActionEvent;
};
