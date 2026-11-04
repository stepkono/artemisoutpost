// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArtemisOutpost/Anchors/AnchorsManagerSubsystem.h"
#include "ArtemisOutpost/GameData/ArtemisGameState.h"
#include "ArtemisOutpost/Miscellaneous/DataTypes.h"
#include "Components/ActorComponent.h"
#include "MapCutoutManager.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCutoutSet); 

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
	void HandleAnchorsUpdate(const FOrderedAnchors& RawAnchors);
	
	UFUNCTION(BlueprintCallable)
	void UpdateMaterialParamCollection() const;
	
	UFUNCTION()
	void BindGeoRefToAnchor() const; 
	
	UFUNCTION()
	void PutGeoRefIntoTablePlane(const FVector& TableCenter, const FVector& TableNormal) const; 
	
	UFUNCTION()
	bool IsAuthoritativeClient() const; 
	
	// The seed: calibrates the table from the anchors and activates the TransformationsManager (TableZ, TargetFrame,
	// XRTransform are frozen here). Runs only when IsReadyToSeed, otherwise it marks the seed pending and the tick
	// retries until the player is in AR.
	UFUNCTION()
	void HandleAnchorsSpawned();

	// The anchors only describe the physical table in the AR pawn's tracking space with no VR tilt. After a reconnect
	// into VR the anchors arrive while the VR char is possessed and the tilt is active, seeding then would freeze a
	// flipped or misplaced table.
	bool IsReadyToSeed() const;

public: 
	FCutoutSet OnCutoutSet; 
	
private:	
	UPROPERTY()
	AArtemisGameState* GS;
	
	UPROPERTY()
	FOrderedAnchorsPositions AnchorPositions;
	
	UPROPERTY(EditAnywhere, Category="Material Collection")
	UMaterialParameterCollection* AnchorsCollection; 
	
	UPROPERTY()
	UAnchorsManagerSubsystem* AnchorsManager = nullptr; 
	
	UPROPERTY()
	float InitialMoonScalingFactor; 
	
	bool bAnchorsSpawned = false;

	// Anchors are located but the player was not in AR yet. The tick is enabled only while this is true.
	bool bSeedPending = false;

	// Consecutive ticks IsReadyToSeed held. After the tilt reset the tracking space follows on the next frame and the
	// anchor actors one frame later, so the seed waits for RequiredReadyTicks.
	int32 SeedReadyTicks = 0;
	static constexpr int32 RequiredReadyTicks = 2;

	UPROPERTY()
	AActor* MoonGeoRef; 
};
