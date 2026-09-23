// Fill out your copyright notice in the Description page of Project Settings.

#include "ControllerRayComponent.h"

#include "Components/WidgetInteractionComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/Pawn.h"
#include "InputCoreTypes.h"
#include "ArtemisOutpost/Networking/ClientServerConnection/NetUtils.h"

UControllerRayComponent::UControllerRayComponent()
{
	// Ticks to feed the beam every frame; the guard in TickComponent keeps remote instances idle
	// unless they are in pointer mode.
	PrimaryComponentTick.bCanEverTick = true;

	// Client-local visual only — never replicated. The shared pointer STATE lives on AArtemisPlayerState
	// and is relayed into SetPointerMode by UPlayerCuesManager on every peer.
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

bool UControllerRayComponent::IsOwnerLocallyControlled() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return Pawn && Pawn->IsLocallyControlled();
}

void UControllerRayComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// The listen-server host has no headset and renders for nobody: never build visuals there.
	if (ArtemisNet::IsServerHost(GetNetMode()))
	{
		return;
	}

	// Local pawn: always (widget rays). Anyone else: only while a hand is in pointer mode.
	const bool bLocal = IsOwnerLocallyControlled();
	if (!bLocal && !HasAnyPointerModeDesired())
	{
		return;
	}

	if (!bSetupDone && !EnsureSetup())
	{
		return;
	}

	for (const FControllerRayState& Ray : Rays)
	{
		// A remote proxy draws ONLY the pointing hand; its widget rays stay invisible.
		if (!bLocal && !Ray.bPointerMode)
		{
			continue;
		}
		if (Ray.bEnabled && Ray.Interaction && Ray.Visual)
		{
			UpdateRayVisual(Ray);
		}
	}
}

void UControllerRayComponent::ScaleRay(float AbsoluteScalingFactor)
{
	CurrentRayScale = FMath::Max(AbsoluteScalingFactor, 0.01f);

	// Push immediately so a scale set from Blueprint is visible this frame, not on the next tick.
	for (FControllerRayState& Ray : Rays)
	{
		if (Ray.Visual && Ray.Visual->GetAsset())
		{
			Ray.Visual->SetVariableFloat(RayScaleParamName, CurrentRayScale);
		}
		// Only the pointer reach follows the scale; the widget reach stays as authored in the BP.
		if (Ray.bPointerMode && Ray.Interaction)
		{
			Ray.Interaction->InteractionDistance = PointerInteractionDistance * CurrentRayScale;
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
		UE_LOG(LogTemp, Warning, TEXT("[ControllerRays] No RayDesigns assigned on %s; rays will trace but draw nothing."), *Owner->GetName());
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
		Ray.AuthoredInteractionDistance = Found->InteractionDistance;

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

	UE_LOG(LogTemp, Log, TEXT("[ControllerRays] %s: set up %d ray(s), %d design(s), pointerDesign=%d, local=%d."),
		*Owner->GetName(), Rays.Num(), RayDesigns.Num(), PointerDesignIndex, IsOwnerLocallyControlled() ? 1 : 0);

	// Apply the desired states now that the rays exist (callers may have set them before setup).
	SetRayEnabled(EControllerRayHand::Left, bDesiredLeftEnabled);
	SetRayEnabled(EControllerRayHand::Right, bDesiredRightEnabled);

	// A remote proxy's widget rays must never show: only the pointing hand is switched on below.
	if (!IsOwnerLocallyControlled())
	{
		for (FControllerRayState& Ray : Rays)
		{
			ApplyEnabled(Ray, false);
		}
	}

	if (FControllerRayState* Left = FindRay(EControllerRayHand::Left))
	{
		ApplyPointerMode(*Left, bDesiredLeftPointer);
	}
	if (FControllerRayState* Right = FindRay(EControllerRayHand::Right))
	{
		ApplyPointerMode(*Right, bDesiredRightPointer);
	}

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
		UE_LOG(LogTemp, Warning, TEXT("[ControllerRays] %s: design index %d has no Niagara system (RayDesigns has %d entries). That ray draws nothing."),
			*GetNameSafe(GetOwner()), DesignIndex, RayDesigns.Num());
		Visual->Deactivate();
	}
}

// ---- Per-frame visual ----

void UControllerRayComponent::UpdateRayVisual(const FControllerRayState& Ray) const
{
	UWidgetInteractionComponent* WI = Ray.Interaction;
	UNiagaraComponent* NC = Ray.Visual;

	// No design assigned (invalid index into RayDesigns): nothing to feed, and feeding an assetless
	// component spams "OverrideParameter(PointArray) System(None) ... was not found".
	if (!NC->GetAsset())
	{
		return;
	}

	const FVector Start = WI->GetComponentLocation();
	const FHitResult Hit = WI->GetLastHitResult();

	// Detect the hit by the presence of a hit component, NOT by bBlockingHit. A WidgetComponent that
	// responds to the interaction trace channel with Overlap comes back as a NON-blocking hit with a
	// valid ImpactPoint; WidgetInteractionComponent stores exactly that in LastHitResult.
	const bool bHit = (Hit.GetComponent() != nullptr);

	const FVector End = bHit
		? FVector(Hit.ImpactPoint)
		: (Start + WI->GetForwardVector() * WI->InteractionDistance);

	// World positions on the moon are far outside float precision; hand the beam to Niagara relative
	// to its own component (attached to the hand) so the numbers stay small. See bFeedPointsInLocalSpace.
	FVector FeedStart = Start;
	FVector FeedEnd   = End;
	if (bFeedPointsInLocalSpace)
	{
		const FTransform NCXform = NC->GetComponentTransform();
		FeedStart = NCXform.InverseTransformPosition(Start);
		FeedEnd   = NCXform.InverseTransformPosition(End);
	}

	TArray<FVector> Points;
	Points.Reserve(2);
	Points.Add(FeedStart);
	Points.Add(FeedEnd);
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(NC, PointArrayParamName, Points);

	NC->SetVariableVec3(EndPointParamName, FeedEnd);
	NC->SetVariableFloat(HitStateParamName, bHit ? 1.0f : 0.0f);
	NC->SetVariableFloat(RayScaleParamName, CurrentRayScale);
}

bool UControllerRayComponent::GetRayHit(EControllerRayHand Hand, FVector& OutStart, FVector& OutDirection, FHitResult& OutHit) const
{
	const FControllerRayState* Ray = FindRay(Hand);
	if (!Ray || !Ray->Interaction)
	{
		return false;
	}
	OutStart     = Ray->Interaction->GetComponentLocation();
	OutDirection = Ray->Interaction->GetForwardVector();
	OutHit       = Ray->Interaction->GetLastHitResult();
	return true;
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
		// A hand in pointer mode keeps its pointer look; the new design becomes what it returns to.
		if (Ray.bPointerMode)
		{
			Ray.SavedDesignIndex = DesignIndex;
			continue;
		}
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
	if (Ray->bPointerMode)
	{
		Ray->SavedDesignIndex = DesignIndex;
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
	if (const FControllerRayState* Ray = FindRay(Hand))
	{
		return Ray->DesignIndex;
	}
	return CurrentDesignIndex;
}

// ---- Enable / suppress ----

void UControllerRayComponent::ApplyEnabled(FControllerRayState& Ray, bool bEnabled) const
{
	Ray.bEnabled = bEnabled;

	if (Ray.Visual)
	{
		Ray.Visual->SetVisibility(bEnabled, true);
		bEnabled ? Ray.Visual->Activate() : Ray.Visual->Deactivate();
	}
	if (Ray.Interaction)
	{
		// Stop tracing/hovering when off, so a suppressed ray can't hover or click a widget.
		Ray.Interaction->SetActive(bEnabled);
	}
}

void UControllerRayComponent::SetRayEnabled(EControllerRayHand Hand, bool bEnabled)
{
	// Remember the desired state so this works before setup and survives (re)setup.
	(Hand == EControllerRayHand::Left ? bDesiredLeftEnabled : bDesiredRightEnabled) = bEnabled;

	FControllerRayState* Ray = FindRay(Hand);
	if (!Ray)
	{
		return; // not set up yet — the desired state above will be applied in EnsureSetup.
	}

	// While pointing, the hand stays on regardless; the desired state is restored when pointing ends.
	if (Ray->bPointerMode)
	{
		Ray->bPointerForcedEnable = !bEnabled;
		return;
	}

	ApplyEnabled(*Ray, bEnabled);
}

void UControllerRayComponent::SetAllRaysEnabled(bool bEnabled)
{
	SetRayEnabled(EControllerRayHand::Left, bEnabled);
	SetRayEnabled(EControllerRayHand::Right, bEnabled);
}

// ---- Pointer mode ----

void UControllerRayComponent::SetPointerMode(EControllerRayHand Hand, bool bOn)
{
	(Hand == EControllerRayHand::Left ? bDesiredLeftPointer : bDesiredRightPointer) = bOn;

	FControllerRayState* Ray = FindRay(Hand);
	UE_LOG(LogTemp, Log, TEXT("[ControllerRays] %s: SetPointerMode(%s, %d) -> ray %s."),
		*GetNameSafe(GetOwner()), Hand == EControllerRayHand::Left ? TEXT("Left") : TEXT("Right"), bOn ? 1 : 0,
		Ray ? TEXT("found") : TEXT("NOT SET UP YET (applied once EnsureSetup runs)"));
	if (!Ray)
	{
		return; // applied in EnsureSetup (the tick guard lets a remote proxy set up now if bOn).
	}
	ApplyPointerMode(*Ray, bOn);
}

void UControllerRayComponent::ApplyPointerMode(FControllerRayState& Ray, bool bOn)
{
	if (Ray.bPointerMode == bOn)
	{
		return;
	}
	Ray.bPointerMode = bOn;

	if (bOn)
	{
		if (Ray.Interaction)
		{
			Ray.Interaction->InteractionDistance = PointerInteractionDistance * CurrentRayScale;
		}

		Ray.SavedDesignIndex = Ray.DesignIndex;
		if (RayDesigns.IsValidIndex(PointerDesignIndex))
		{
			Ray.DesignIndex = PointerDesignIndex;
			ApplyDesignToVisual(Ray.Visual, Ray.DesignIndex);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[ControllerRays] PointerDesignIndex %d is not a valid RayDesigns entry; pointer keeps the current look."), PointerDesignIndex);
		}

		// The pointer must be visible even if this hand's widget ray is currently suppressed (tool
		// active, remote proxy). Remember that we forced it so the exit restores the suppression.
		Ray.bPointerForcedEnable = !Ray.bEnabled;
		if (Ray.bPointerForcedEnable)
		{
			ApplyEnabled(Ray, true);
		}
	}
	else
	{
		if (Ray.Interaction)
		{
			// Back to the BP-authored widget reach.
			Ray.Interaction->InteractionDistance = Ray.AuthoredInteractionDistance;
		}

		Ray.DesignIndex = Ray.SavedDesignIndex;
		ApplyDesignToVisual(Ray.Visual, Ray.DesignIndex);

		if (Ray.bPointerForcedEnable)
		{
			Ray.bPointerForcedEnable = false;
			// Remote proxies never show widget rays; the local pawn returns to its desired state.
			const bool bDesired = IsOwnerLocallyControlled()
				&& (Ray.Hand == EControllerRayHand::Left ? bDesiredLeftEnabled : bDesiredRightEnabled);
			ApplyEnabled(Ray, bDesired);
		}
	}
}

bool UControllerRayComponent::IsPointerModeActive(EControllerRayHand Hand) const
{
	const FControllerRayState* Ray = FindRay(Hand);
	return Ray ? Ray->bPointerMode : (Hand == EControllerRayHand::Left ? bDesiredLeftPointer : bDesiredRightPointer);
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

const FControllerRayState* UControllerRayComponent::FindRay(EControllerRayHand Hand) const
{
	for (const FControllerRayState& Ray : Rays)
	{
		if (Ray.Hand == Hand)
		{
			return &Ray;
		}
	}
	return nullptr;
}
