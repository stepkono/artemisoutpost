// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VisualScaleSyncComponent.generated.h"

class UTransformationsManager;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnVisualScaleChanged, float, NewScale, float, PreviousScale);

/**
 * Keeps an AR actor in step with the AR zoom (UTransformationsManager's committed visual scale) and fires
 * OnVisualScaleChanged whenever the owner has to rescale / reposition itself.
 *
 * Two paths, one result:
 *  - Event: UTransformationsManager::OnWTMShouldResize, which fires on the frame the camera adopts a new scale. Keeps
 *    the owner exactly in step while zooming.
 *  - Poll: every tick the committed scale is compared with the last applied one. Catches everything the event cannot
 *    reach: actors spawned after the last zoom (lazy cue spawn, respawn after a reconnect) and missed broadcasts.
 * Both go through ApplyScale, so a scale the event already applied is never applied again by the poll. The very first
 * tick always fires, so a freshly spawned owner is synced at once (PreviousScale = 1, the unzoomed design scale).
 *
 * Use it instead of binding OnWTMShouldResize yourself, never together with such a binding (double adjustment).
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class ARTEMISOUTPOST_API UVisualScaleSyncComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVisualScaleSyncComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Fired with the new committed scale and the one applied before it (1 on the first sync).
	UPROPERTY(BlueprintAssignable, Category = "Visual Scale Sync")
	FOnVisualScaleChanged OnVisualScaleChanged;

	// The scale the owner was last told to adopt. 1 until the first sync.
	UFUNCTION(BlueprintPure, Category = "Visual Scale Sync")
	float GetAppliedVisualScale() const { return AppliedVisualScale; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleWTMShouldResize(float AbsoluteScalingFactor);

	void ApplyScale(float NewScale);

	// Resolved lazily: the manager is a world subsystem and exists in every world, but stays inactive outside AR.
	UTransformationsManager* GetTransformationsManager();

	UPROPERTY(Transient)
	TObjectPtr<UTransformationsManager> TransformationsManager;

	float AppliedVisualScale = 1.0f;
	bool bHasAppliedVisualScale = false;
	bool bBoundToResizeEvent = false;
};
