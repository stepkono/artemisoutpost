// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SpatialAnchorModel.generated.h"

class UMaterialInstanceDynamic;
class UPrimitiveComponent;

UCLASS()
class ARTEMISOUTPOST_API ASpatialAnchorModel : public AActor
{
	GENERATED_BODY()

public:
	ASpatialAnchorModel();

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;

	virtual void OnConstruction(const FTransform& Transform) override;

public:
	/** Component used to anchor the floating context info widget. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SpatialAnchorModel|Components")
	TObjectPtr<USceneComponent> ContextInfoPoint;

	/**
	 * If true, this is a transient preview/temp model that is not backed by a real spatial
	 * anchor and will be moved/destroyed by the manager. Set on spawn by the manager.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAnchorModel", meta = (ExposeOnSpawn = "true"))
	bool bIsTemp = false;

	/** Anchor icon (foreground) component the dynamic icon material is applied to. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SpatialAnchorModel|Components")
	TObjectPtr<UPrimitiveComponent> AnchorBackground;

	/** Dynamic material instance for the anchor icon (slot 0). */
	UPROPERTY(BlueprintReadOnly, Category = "SpatialAnchorModel|Materials")
	TObjectPtr<UMaterialInstanceDynamic> AnchorIcon;

	/** Dynamic material instance for the anchor background (slot 1). */
	UPROPERTY(BlueprintReadOnly, Category = "SpatialAnchorModel|Materials")
	TObjectPtr<UMaterialInstanceDynamic> AnchorBG;

protected:
	UPROPERTY()
	UOculusXRAnchorComponent* SpatialAnchorComponent; 
	
private:
	FRotator GetRotationToCamera(const FVector& WorldPosition) const;
	void UpdateContextInfoLocation() const;

public: 
	/** Spawn / show the floating context info widget once the anchor has been saved. */
	UFUNCTION(BlueprintCallable, Category = "SpatialAnchorModel")
	void AddContextInfo();

	/** Tear down the context info widget when the anchor is being erased. */
	UFUNCTION(BlueprintCallable, Category = "SpatialAnchorModel")
	void RemoveContextInfo();

	/** Refresh material parameters that reflect anchor selection / hover state. */
	UFUNCTION(BlueprintCallable, Category = "SpatialAnchorModel")
	void UpdateColorFeedback();

	UFUNCTION(BlueprintCallable, Category = "SpatialAnchorComponent")
	UOculusXRAnchorComponent* GetSpatialAnchorComponent(); 
	
private:
	UPROPERTY()
	float ContextInfoPointOffsetZ;
};
