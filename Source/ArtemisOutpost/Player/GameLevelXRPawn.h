// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArtemisOutpost/GameData/ArtemisGameState.h"
#include "GameFramework/Pawn.h"
#include "GameLevelXRPawn.generated.h"

UCLASS()
class ARTEMISOUTPOST_API AGameLevelXRPawn : public APawn
{
	GENERATED_BODY()

public:
	// Sets default values for this pawn's properties
	AGameLevelXRPawn();

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

private: 
	UFUNCTION(BlueprintCallable)
	bool IsAuthoritativeClient() const; 
	
	UFUNCTION(Server, Reliable)
	void ShareAnchorsWithServer(FCustomAnchors RawAnchors); 
	
private: 
	UPROPERTY()
	UArtemisGameInstance* GI; 
};
