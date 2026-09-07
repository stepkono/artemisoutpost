// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"
#include "ArtemisOutpost/MiniGames/General/MinigameTypes.h"
#include "ArtemisOutpost/StudyData/Types/ProviderDataBase.h"
#include "MiniGameProviderData.generated.h"

// UProviderDataBase payload mirroring FMiniGameRecord (see MinigameTypes.h), for providers that
// broadcast minigame/building data through the aggregator. Kept as a separate type on purpose:
// FMiniGameRecord stays the lightweight value type MoonBuildingsManager/SignalTower/MinigameActor
// pass around, this is only what goes out over the provider/aggregator delegate chain.
UCLASS(BlueprintType)
class ARTEMISOUTPOST_API UMiniGameProviderData : public UProviderDataBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Buildings")
	FGuid MGID;

	UPROPERTY(BlueprintReadOnly, Category = "Buildings")
	FVector BuildLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Buildings")
	FString BuiltByUPID;

	UPROPERTY(BlueprintReadOnly, Category = "Buildings")
	EMinigameState State = EMinigameState::Idle;

	UPROPERTY(BlueprintReadOnly, Category = "Buildings")
	EMiniGameType Type = EMiniGameType::SignalTower;

	// Type-specific data (e.g. FHabitatData). Empty for types without extra data.
	UPROPERTY(BlueprintReadOnly, Category = "Buildings")
	FInstancedStruct MiniGameData;
	
	virtual TSharedPtr<FJsonObject> BuildJsonFromData(const EGameEventType GameEventType) override;
	TSharedPtr<FJsonObject> SerializeNewMinigameData();
};
