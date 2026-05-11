// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArtemisOutpost/AnchorsManagerSubsystem.h"
#include "ArtemisOutpost/DataTypes.h"
#include "ArtemisOutpost/GameData/ArtemisGameState.h"
#include "Components/ActorComponent.h"
#include "MapCutoutManager.generated.h"

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
	
	UFUNCTION(BlueprintCallable)
	void SetMaterialCollection(UMaterialParameterCollection* MaterialParameterCollection);
	
	UFUNCTION(BlueprintCallable)
	void UpdateMaterialParamCollection();
	
	UFUNCTION()
	void BindGeoRefToAnchor(AActor* SpawnedAnchor); 
	
	UFUNCTION()
	void PutGeoRefIntoTablePlane(const FVector& TableCenter, const FVector& TableNormal); 
	
private:	
	UPROPERTY()
	AArtemisGameState* GS;
	
	UPROPERTY()
	FAnchorsPositions AnchorPositions;
	
	UPROPERTY()
	UMaterialParameterCollection* AnchorsCollection; 
	
	UPROPERTY()
	UAnchorsManagerSubsystem* AnchorsManager = nullptr; 
	
	UPROPERTY()
	float InitialMoonScalingFactor; 
};
