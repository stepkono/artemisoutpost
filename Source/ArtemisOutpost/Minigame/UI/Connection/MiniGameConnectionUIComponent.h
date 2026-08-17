// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArtemisOutpost/Player/PawnController.h"
#include "Components/WidgetComponent.h"

class AMinigameActor;
#include "MiniGameConnectionUIComponent.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ARTEMISOUTPOST_API UMiniGameConnectionUIComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UMiniGameConnectionUIComponent();
	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
							   FActorComponentTickFunction* ThisTickFunction) override;
	
	void SetShowConnectionUI(bool bShowConnectionUI);

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

private:
	// Resolves (and caches) THIS client's controller; the prompt is a local-only visual.
	APawnController* GetLocalController();

	// Moon surface normal at the tower (the tower's own up axis); the prompt uses it as its
	// upright axis instead of world up, since up != world-Z on the moon sphere.
	FVector GetMoonUp() const;

	UFUNCTION()
	void FacePlayer();

	UFUNCTION()
	void MoveToPlayer();
	
	UFUNCTION()
	void RequestEnter(); 
	
protected:
	UPROPERTY()
	bool bShowConnectionUI;
	
	UPROPERTY(BlueprintReadOnly, Category = "Player")
	APawnController* PC;
	
	UPROPERTY(EditAnywhere, Category = "Widget Content")
	FString ButtonText;

	// --- Prompt placement (explicit, in cm) ---

	// Clearance added to the actor's own footprint radius to get the orbit radius, so the prompt
	// always sits a fixed gap OUTSIDE the actor regardless of its size (2-3 m works well).
	UPROPERTY(EditAnywhere, Category = "Prompt Placement")
	float Margin = 250.0f;

	// If false the prompt stays at a fixed spot in front of the tower and only rotates to face
	// the viewer (no orbit tracking).
	UPROPERTY(EditAnywhere, Category = "Prompt Placement")
	bool bTrackPlayer = true;

	// Draws a debug sphere at the computed prompt location to find it when it isn't visible.
	UPROPERTY(EditAnywhere, Category = "Prompt Placement")
	bool bDrawDebug = false;

private:
	UPROPERTY()
	AMinigameActor* Owner;

	// Actor footprint radius (max horizontal half-extent), cached once in BeginPlay. Computed from
	// colliding components only, which conveniently excludes this widget from its own basis.
	float ActorRadius = 0.0f;

	// Offset from the actor pivot to the bounds CENTER (world space), cached in BeginPlay. The orbit
	// is anchored here, not at the pivot: a pivot at the tower's foot would otherwise sink the
	// prompt into the ground. Uses BoxExtent.Z via the bounds origin.
	FVector CenterOffset = FVector::ZeroVector;
};
