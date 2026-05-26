// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "OculusXRAnchorTypes.h"
#include "ArtemisOutpost/GameData/ArtemisGameInstance.h"
#include "AnchorSaveGame.generated.h"

/**
 * Persistent record of a single saved spatial anchor.
 * Mirrors the BP_AnchorSaveGame "SavedAnchor" struct.
 */
USTRUCT(BlueprintType)
struct ARTEMISOUTPOST_API FSavedAnchor
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saved Anchor")
	FOculusXRUUID UUID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saved Anchor")
	bool bIsSavedLocal = true;
};

/**
 * SaveGame container for spatial anchor UUIDs that should persist between sessions.
 * Loaded by USpatialAnchorManager::LoadUUIDsFromFile().
 */
UCLASS(BlueprintType)
class ARTEMISOUTPOST_API UAnchorSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	/** Used by the Blueprint anchor manager — unordered list of locally saved anchors. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchor Save Game")
	TArray<FSavedAnchor> SavedAnchors;

	/** Used by the server — ordered A/B/C/D anchor UUIDs persisted between sessions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchor Save Game")
	FOrderedAnchors ServerAnchors;
};
