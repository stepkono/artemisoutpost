// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Miscellaneous/DataTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "TransformationsManager.generated.h"

/**
 * 
 */
UCLASS()
class ARTEMISOUTPOST_API UTransformationsManager : public UTickableWorldSubsystem
{
	GENERATED_BODY()
	
public: 
#pragma region Constructors
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
#pragma endregion
	
	UFUNCTION()
	void OnNewBaseCoordinates(FMapBaseCoordinates& BaseCoordinate);
};
