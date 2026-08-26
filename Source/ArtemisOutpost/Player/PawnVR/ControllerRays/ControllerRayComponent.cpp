// Fill out your copyright notice in the Description page of Project Settings.

#include "ControllerRayComponent.h"

#include "Components/WidgetInteractionComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/Pawn.h"
#include "InputCoreTypes.h"

UControllerRayComponent::UControllerRayComponent()
{
	// Ticks to feed the beam every frame; only does work on the locally-controlled pawn.
	PrimaryComponentTick.bCanEverTick = true;

	// Client-local visual only — never replicated. (A shared "point at things for others" ray is a
	// separate opt-in feature that belongs on the client-owned APawnController.)
	SetIsReplicatedByDefault(false);
}

void UControllerRayComponent::BeginPlay()
{
	Super::BeginPlay();

	CurrentDesignIndex = RayDesigns.IsValidIndex(DefaultDesignIndex) ? DefaultDesignIndex : 0;
}

void UControllerRayComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (FControllerRayState& Ray : Rays)
	{
		if (Ray.Visual)
		{
			Ray.Visual->DestroyComponent();
			Ray.Visual = nullptr;
		}
	}
	Rays.Reset();

	Super::EndPlay(EndPlayReason);
}

void UControllerRayComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Local player only. On a networked client possession lands AFTER BeginPlay, so setup is deferred
	// to the first tick where local control is actually established. Proxies / server early-out cheaply.
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !Pawn->IsLocallyControlled())
	{
		return;
	}

	if (!bSetupDone && !EnsureSetup())
	{
		return;
	}

	for (const FControllerRayState& Ray : Rays)
	{
		if (Ray.bEnabled && Ray.Interaction && Ray.Visual)
		{
			UpdateRayVisual(Ray);
		}
	}
}

// ---- Setup ----

bool UControllerRayComponent::EnsureSetup()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}

	TArray<UWidgetInteractionComponent*> Interactions;
	Owner->GetComponents<UWidgetInteractionComponent>(Interactions);
	if (Interactions.Num() == 0)
	{
		return false; // BP components not present yet — try again next tick.
	}

	if (RayDesigns.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ControllerRays] No RayDesigns assigned; rays will trace but draw nothing."));
	}

	auto AddRay = [&](EControllerRayHand Hand, FName Tag)
	{
		UWidgetInteractionComponent* Found = nullptr;
		for (UWidgetInteractionComponent* WI : Interactions)
		{
			if (WI && WI->ComponentHasTag(Tag))
			{
				Found = WI;
				break;
			}
		}
		if (!Found)
		{
			UE_LOG(LogTemp, Warning, TEXT("[ControllerRays] No WidgetInteractionComponent tagged '%s' on %s; that hand's ray is off."),
				*Tag.ToString(), *Owner->GetName());
			return;
		}

		FControllerRayState Ray;
		Ray.Hand = Hand;
		Ray.Interaction = Found;
		Ray.DesignIndex = CurrentDesignIndex;

		UNiagaraComponent* NC = NewObject<UNiagaraComponent>(Owner, NAME_None, RF_Transient);
		NC->SetAutoActivate(false);
		NC->AttachToComponent(Found, FAttachmentTransformRules::SnapToTargetIncludingScale);
		NC->RegisterComponent();
		ApplyDesignToVisual(NC, Ray.DesignIndex);
		Ray.Visual = NC;

		Rays.Add(Ray);
	};

	AddRay(EControllerRayHand::Left, LeftInteractionTag);
	AddRay(EControllerRayHand::Right, RightInteractionTag);

	bSetupDone = true;

	// Apply the desired enable state now that the rays exist (BP may have called SetRayEnabled early).
	SetRayEnabled(EControllerRayHand::Left, bDesiredLeftEnabled);
	SetRayEnabled(EControllerRayHand::Right, bDesiredRightEnabled);

	return Rays.Num() > 0;
}

void UControllerRayComponent::ApplyDesignToVisual(UNiagaraComponent* Visual, int32 DesignIndex) const
{
	if (!Visual)
	{
		return;
	}

	UNiagaraSystem* Design = RayDesigns.IsValidIndex(DesignIndex) ? RayDesigns[DesignIndex].Get() : nullptr;
	Visual->SetAsset(Design);
	if (Design)
	{
		Visual->Activate(true);
	}
	else
	{
		Visual->Deactivate();
	}
}

// ---- Per-frame visual ----

void UControllerRayComponent::UpdateRayVisual(const FControllerRayState& Ray) const
{
	UWidgetInteractionComponent* WI = Ray.Interaction;
	UNiagaraComponent* NC = Ray.Visual;

	const FVector Start = WI->GetComponentLocation();
	const FHitResult Hit = WI->GetLastHitResult();

	// Detect the widget hit by the presence of a hit component, NOT by bBlockingHit. A WidgetComponent
	// that responds to the interaction trace channel with Overlap (the common setup — it's what makes
	// the debug ray stop on the widget) comes back from LineTraceMultiByChannel as a NON-blocking hit:
	// bBlockingHit == false, but ImpactPoint is valid. WidgetInteractionComponent stores exactly that
	// hit in LastHitResult, so GetComponent() != null iff the ray is on a widget this frame.
	const bool bHit = (Hit.GetComponent() != nullptr);

	// Hit.ImpactPoint is an FVector_NetQuantize; make both ternary branches a plain FVector.
	const FVector End = bHit
		? FVector(Hit.ImpactPoint)
		: (Start + WI->GetForwardVector() * WI->InteractionDistance);

	TArray<FVector> Points;
	Points.Reserve(2);
	Points.Add(Start);
	Points.Add(End);
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(NC, PointArrayParamName, Points);

	// End / impact point as a standalone vector, so an endpoint-sphere emitter can sit there without
	// reading the array; and a 0/1 flag for colour-on-hit. Both are no-ops if the design lacks them.
	NC->SetVariableVec3(EndPointParamName, End);
	NC->SetVariableFloat(HitStateParamName, bHit ? 1.0f : 0.0f);
}

// ---- Design switching ----

void UControllerRayComponent::SetDesign(int32 DesignIndex)
{
	if (!RayDesigns.IsValidIndex(DesignIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("[ControllerRays] SetDesign: invalid index %d (have %d designs)."), DesignIndex, RayDesigns.Num());
		return;
	}

	// Also becomes the default for rays created later (deferred setup).
	CurrentDesignIndex = DesignIndex;
	for (FControllerRayState& Ray : Rays)
	{
		Ray.DesignIndex = DesignIndex;
		ApplyDesignToVisual(Ray.Visual, DesignIndex);
	}
}

void UControllerRayComponent::SetHandDesign(EControllerRayHand Hand, int32 DesignIndex)
{
	if (!RayDesigns.IsValidIndex(DesignIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("[ControllerRays] SetHandDesign: invalid index %d (have %d designs)."), DesignIndex, RayDesigns.Num());
		return;
	}

	FControllerRayState* Ray = FindRay(Hand);
	if (!Ray)
	{
		return;
	}
	Ray->DesignIndex = DesignIndex;
	ApplyDesignToVisual(Ray->Visual, DesignIndex);
}

void UControllerRayComponent::NextDesign()
{
	if (RayDesigns.Num() == 0)
	{
		return;
	}
	SetDesign((CurrentDesignIndex + 1) % RayDesigns.Num());
}

int32 UControllerRayComponent::GetHandDesignIndex(EControllerRayHand Hand) const
{
	for (const FControllerRayState& Ray : Rays)
	{
		if (Ray.Hand == Hand)
		{
			return Ray.DesignIndex;
		}
	}
	return CurrentDesignIndex;
}

// ---- Enable / suppress ----

void UControllerRayComponent::SetRayEnabled(EControllerRayHand Hand, bool bEnabled)
{
	// Remember the desired state so this works before setup and survives (re)setup.
	(Hand == EControllerRayHand::Left ? bDesiredLeftEnabled : bDesiredRightEnabled) = bEnabled;

	FControllerRayState* Ray = FindRay(Hand);
	if (!Ray)
	{
		return; // not set up yet — the desired state above will be applied in EnsureSetup.
	}

	Ray->bEnabled = bEnabled;

	if (Ray->Visual)
	{
		Ray->Visual->SetVisibility(bEnabled, true);
		bEnabled ? Ray->Visual->Activate() : Ray->Visual->Deactivate();
	}
	if (Ray->Interaction)
	{
		// Stop tracing/hovering when off, so a suppressed ray can't hover or click a widget.
		Ray->Interaction->SetActive(bEnabled);
	}
}

void UControllerRayComponent::SetAllRaysEnabled(bool bEnabled)
{
	SetRayEnabled(EControllerRayHand::Left, bEnabled);
	SetRayEnabled(EControllerRayHand::Right, bEnabled);
}

// ---- Input ----

void UControllerRayComponent::BindInput(UEnhancedInputComponent* EnhancedInputComponent)
{
	if (!EnhancedInputComponent)
	{
		return;
	}

	if (LeftClickAction)
	{
		EnhancedInputComponent->BindAction(LeftClickAction, ETriggerEvent::Started, this, &UControllerRayComponent::HandleLeftPressed);
		EnhancedInputComponent->BindAction(LeftClickAction, ETriggerEvent::Completed, this, &UControllerRayComponent::HandleLeftReleased);
	}
	if (RightClickAction)
	{
		EnhancedInputComponent->BindAction(RightClickAction, ETriggerEvent::Started, this, &UControllerRayComponent::HandleRightPressed);
		EnhancedInputComponent->BindAction(RightClickAction, ETriggerEvent::Completed, this, &UControllerRayComponent::HandleRightReleased);
	}
}

void UControllerRayComponent::PressPointer(EControllerRayHand Hand)
{
	FControllerRayState* Ray = FindRay(Hand);
	if (Ray && Ray->bEnabled && Ray->Interaction)
	{
		Ray->Interaction->PressPointerKey(EKeys::LeftMouseButton);
	}
}

void UControllerRayComponent::ReleasePointer(EControllerRayHand Hand)
{
	FControllerRayState* Ray = FindRay(Hand);
	if (Ray && Ray->Interaction)
	{
		// Always release, even if disabled since press, so a pointer key can't get stuck down.
		Ray->Interaction->ReleasePointerKey(EKeys::LeftMouseButton);
	}
}

FControllerRayState* UControllerRayComponent::FindRay(EControllerRayHand Hand)
{
	for (FControllerRayState& Ray : Rays)
	{
		if (Ray.Hand == Hand)
		{
			return &Ray;
		}
	}
	return nullptr;
}
