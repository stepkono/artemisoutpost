// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RadialWidget.generated.h"

class UImage;
class UMaterialInstanceDynamic;

// Reusable radial progress ring. Visualizes a 0..360 angle as a ring that fills
// clockwise from 12 o'clock. Deliberately game-agnostic: it knows nothing about
// axes, minigames or players — callers just push an absolute angle via SetAngle().
//
// The concrete widget tree (the Image using M_ProgressBarMaterialInstance) lives in
// the BP child WBP_RadialWidget, which reparents to this class. Abstract because the
// C++ side cannot author the Image; it only drives the material.
UCLASS(Abstract)
class ARTEMISOUTPOST_API URadialWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Sets the fill from an absolute angle in degrees [0..360].
	// Fill = Degrees/360 is written to the material's "Percent" scalar. Safe to call
	// before NativeConstruct (no-op until the MID exists).
	UFUNCTION(BlueprintCallable, Category = "Radial")
	void SetAngle(float Degrees);

	// Optional runtime override of the ring's fill color (material "Color" vector).
	// Prefer setting the default in M_ProgressBarMaterialInstance; use this only when
	// the color must change per instance/state.
	UFUNCTION(BlueprintCallable, Category = "Radial")
	void SetFillColor(FLinearColor Color);

protected:
	virtual void NativeConstruct() override;

	// The Image whose brush uses M_ProgressBarMaterialInstance. Must be named exactly
	// "RadialImage" in the WBP_RadialWidget hierarchy for BindWidget to resolve it.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> RadialImage;

private:
	// Lazily creates (once) and returns the MID from the Image brush, or null if the
	// Image is unbound / has no material. Makes SetAngle work regardless of call order.
	UMaterialInstanceDynamic* EnsureMID();

	// Dynamic instance created from the Image brush material; parameters are set on this.
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> RadialMID;

	// Material parameter names — must match M_ProgressBar exactly.
	static const FName PercentParam;
	static const FName ColorParam;
};
