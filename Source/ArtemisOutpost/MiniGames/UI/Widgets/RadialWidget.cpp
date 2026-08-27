// Fill out your copyright notice in the Description page of Project Settings.

#include "RadialWidget.h"

#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"

const FName URadialWidget::PercentParam(TEXT("Percent"));
const FName URadialWidget::ColorParam(TEXT("Color"));

void URadialWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Eager attempt so the ring is ready immediately. Lazy path in EnsureMID() covers
	// calls that arrive before this (e.g. SetAngle from BP Event Construct, which fires
	// from inside Super::NativeConstruct() above — i.e. before this line).
	EnsureMID();
}

UMaterialInstanceDynamic* URadialWidget::EnsureMID()
{
	// GetDynamicMaterial() creates (once) a MID from the Image brush's material —
	// M_ProgressBarMaterialInstance set in the designer — and swaps the brush to use
	// it. Returns null only if RadialImage is unbound or its brush has no material.
	if (!RadialMID && RadialImage)
	{
		RadialMID = RadialImage->GetDynamicMaterial();
	}
	return RadialMID;
}

void URadialWidget::SetAngle(float Degrees)
{
	if (UMaterialInstanceDynamic* MID = EnsureMID())
	{
		const float Fill = FMath::Clamp(Degrees / 360.0f, 0.0f, 1.0f);
		MID->SetScalarParameterValue(PercentParam, Fill);
	}
}

void URadialWidget::SetFillColor(FLinearColor Color)
{
	if (UMaterialInstanceDynamic* MID = EnsureMID())
	{
		MID->SetVectorParameterValue(ColorParam, Color);
	}
}
