// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArtemisOutpost/AnchorsManagerSubsystem.h"
#include "ArtemisOutpost/GameData/ArtemisGameState.h"
#include "Components/ActorComponent.h"
#include "MapCutoutManager.generated.h"

USTRUCT()
struct FAnchorsPositions
{
	GENERATED_BODY()
	
	UPROPERTY()
	FVector AAnchorPos; 
	
	UPROPERTY()
	FVector BAnchorPos;
	
	UPROPERTY()
	FVector CAnchorPos;
	
	UPROPERTY()
	FVector DAnchorPos;
};

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class ARTEMISOUTPOST_API UMapCutoutManager : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UMapCutoutManager();
	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

private: 
	UFUNCTION()
	void HandleAnchorsUpdate(const FCustomAnchors& RawAnchors);
	
	UFUNCTION()
	void CalibrateAnchors(FAnchorsPositions* RawAnchorsPositions); 
	
private:	
	UPROPERTY()
	AArtemisGameState* GS;
	
	UPROPERTY()
	FAnchorsPositions AnchorPositions;
	
	UPROPERTY()
	UAnchorsManagerSubsystem* AnchorsManager = nullptr; 
};
