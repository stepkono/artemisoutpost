// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "ClientIdentitySave.generated.h"

/**
 * 
 */
UCLASS()
class ARTEMISOUTPOST_API UClientIdentitySave : public USaveGame
{
	GENERATED_BODY()
	
public: 
	UFUNCTION()
	void WriteUPID(FString NewUPID); 
	
	UFUNCTION()
	FString GetUPID(); 
	
private:
	UPROPERTY()
	FString UPID; 
};
