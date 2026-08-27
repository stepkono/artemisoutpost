// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArtemisOutpost/MiniGames/General/GameInstance/CoupledAxisMinigameActor.h"
#include "SignalTower.generated.h"

// The radio mast (Funkmast). Two coupled alignment axes — [0] toward Earth, [1] toward a Habitat
// (§8.7). Earth is a random bearing every time (never memorizable). Habitat would derive from the
// chosen habitat's geodetic bearing, but there is no habitat data model yet, so it is a random
// placeholder for now. Meshes, the world-space prompt and the AR/VR beams live on BP_SignalTower.
UCLASS()
class ARTEMISOUTPOST_API ASignalTower : public ACoupledAxisMinigameActor
{
	GENERATED_BODY()

public:
	static constexpr int32 AxisEarth = 0;
	static constexpr int32 AxisHabitat = 1;

protected:
	virtual int32 GetAxisCount() const override;
	virtual void InitAxisTargets(TArray<FAxisData>& InAxes) const override;
	virtual bool CanStart(const FString& UPID, FText& OutReason) const override;
	virtual void OnComplete() override;

	// Fired server-side when both axes are aligned. BP hooks habitat activation, score,
	// influence-radius growth, VFX.
	UFUNCTION(BlueprintImplementableEvent, Category = "Signal Tower")
	void OnTowerActivated();
};
