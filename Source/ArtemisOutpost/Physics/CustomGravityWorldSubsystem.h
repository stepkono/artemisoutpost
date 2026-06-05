// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CustomGravityAsyncCallback.h"
#include "GravityAttractorComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "CustomGravityWorldSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class ARTEMISOUTPOST_API UCustomGravityWorldSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
	
public: 	
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual TStatId GetStatId() const override;
	
	virtual void Tick(float DeltaTime) override;
	
	// Keep track of any attractors
	void AddAttractor(UGravityAttractorComponent* GravityAttractorComponent); 
	void RemoveAttractor(UGravityAttractorComponent* GravityAttractorComponent);
	
	virtual void RegisterAsyncCallback(); 
	virtual bool IsAsyncCallbackRegistered() const;
	void AddGravityAttractorData(const FGravityAttractorData& InputData) const; 
	FCustomGravityAsyncCallback* AsyncCallback = nullptr; 

protected:
	TArray<TWeakObjectPtr<UGravityAttractorComponent>> Attractors;	
};
