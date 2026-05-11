// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArtemisGameInstance.h"
#include "GameFramework/GameState.h"
#include "ArtemisGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAnchorsUpdated, const FCustomAnchors&, RawAnchors); 

UCLASS()
class ARTEMISOUTPOST_API AArtemisGameState : public AGameStateBase
{
	GENERATED_BODY()
	
public: 
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
public: 
	FOnAnchorsUpdated OnRawAnchorsUpdated; 
	
private: 
	UPROPERTY(ReplicatedUsing=OnRep_RawAnchors)
	FCustomAnchors RawAnchors;
	
	UFUNCTION()
	void OnRep_RawAnchors();
};
