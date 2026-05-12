// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArtemisOutpost/Anchors/AnchorsManagerSubsystem.h"
#include "ArtemisOutpost/GameData/ArtemisGameState.h"
#include "ArtemisOutpost/Miscellaneous/DataTypes.h"
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
	void CalibrateAnchors(FAnchorsPositions& RawAnchorsPositions); 
	
	UFUNCTION(BlueprintCallable)
	void UpdateMaterialParamCollection() const;
	
	UFUNCTION()
	void BindGeoRefToAnchor(AActor* SpawnedAnchor) const; 
	
	UFUNCTION()
	void PutGeoRefIntoTablePlane(const FVector& TableCenter, const FVector& TableNormal) const; 
	
	UFUNCTION()
	bool IsAuthoritativeClient() const; 
	
private:	
	UPROPERTY()
	AArtemisGameState* GS;
	
	UPROPERTY()
	FAnchorsPositions AnchorPositions;
	
	UPROPERTY(EditAnywhere, Category="Material Collection")
	UMaterialParameterCollection* AnchorsCollection; 
	
	UPROPERTY()
	UAnchorsManagerSubsystem* AnchorsManager = nullptr; 
	
	UPROPERTY()
	float InitialMoonScalingFactor; 
};
