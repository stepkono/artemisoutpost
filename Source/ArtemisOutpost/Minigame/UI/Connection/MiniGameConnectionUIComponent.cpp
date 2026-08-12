// Fill out your copyright notice in the Description page of Project Settings.


#include "MiniGameConnectionUIComponent.h"
#include "MiniGameConnectionUI.h"
#include "ArtemisOutpost/Minigame/General/GameInstance/MinigameActor.h"
#include "ArtemisOutpost/Player/ACharVR.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/PlayerController.h"


// Sets default values for this component's properties
UMiniGameConnectionUIComponent::UMiniGameConnectionUIComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UMiniGameConnectionUIComponent::BeginPlay()
{
	Super::BeginPlay();
		
	this->SetHiddenInGame(false);
	
	Owner = Cast<AMinigameActor>(GetOwner());
	if (!Owner)
	{
		UE_LOG(LogTemp, Error, TEXT("ConnectionUIHolder: Failed to cast Owner as MiniGameOwner."));
		return;
	}

	// Footprint radius from colliding components only (excludes this non-colliding widget), so the
	// orbit sits outside actors of any size. Cached once to avoid a bounds<->position feedback loop.
	FVector Origin;
	FVector BoxExtent;
	Owner->GetActorBounds(true, Origin, BoxExtent);
	ActorRadius = FMath::Max(BoxExtent.X, BoxExtent.Y);

	// Anchor the orbit at the bounds center (pivot may sit at the foot → prompt would clip ground).
	CenterOffset = Origin - Owner->GetActorLocation();

	if (UMiniGameConnectionUI* ConnectionUI = Cast<UMiniGameConnectionUI>(GetWidget()))
	{
		ConnectionUI->SetOwner(Owner);
		ConnectionUI->SetButtonText(ButtonText); 
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ConnectionUIHolder: Failed to cast Widget to MiniGameConnectionUI."));
	}
	
	SetWorldLocation(Owner->GetActorLocation());
}


// Called every frame
void UMiniGameConnectionUIComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bShowConnectionUI)
	{
		//return;
	}
	
	FacePlayer(); 
	MoveToPlayer(); 
}

APawnController* UMiniGameConnectionUIComponent::GetLocalController()
{
	// The Connect-Prompt is a purely local visual, so it tracks THIS client's controller.
	if (!PC)
	{
		const UWorld* World = GetWorld();
		PC = World ? Cast<APawnController>(World->GetFirstPlayerController()) : nullptr;
	}
	return PC;
}

FVector UMiniGameConnectionUIComponent::GetMoonUp() const
{
	// Moon surface normal at the tower. The tower is placed upright on the surface, so its own up
	// axis IS the local moon normal — no georeference lookup needed. (If towers are ever placed
	// without surface orientation, swap this for UGeoUtils::GetUpVector.)
	return Owner ? Owner->GetActorUpVector() : FVector::UpVector;
}

void UMiniGameConnectionUIComponent::FacePlayer()
{
	const APawnController* Controller = GetLocalController();
	if (!Controller)
	{
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);

	// Face the viewer but stay UPRIGHT on the moon surface: yaw only around the moon normal.
	// Projecting ToView onto the tangent plane drops the pitch that otherwise tilted the panel
	// to face up/down (why it was only readable from underneath). World up is meaningless here.
	const FVector Up = GetMoonUp();
	const FVector ToView = ViewLocation - GetComponentLocation();
	const FVector Forward = FVector::VectorPlaneProject(ToView, Up).GetSafeNormal();
	if (Forward.IsNearlyZero())
	{
		return;
	}

	SetWorldRotation(FRotationMatrix::MakeFromXZ(Forward, Up).Rotator());
}

void UMiniGameConnectionUIComponent::MoveToPlayer()
{
	if (!Owner)
	{
		return;
	}

	// Anchor at the bounds CENTER, not the pivot, so the sight line starts at mid-height.
	const FVector Center = Owner->GetActorLocation() + CenterOffset;

	// Direction = the FULL sight line from the actor center to the player camera, normalized. The
	// prompt rides a sphere around the center along this exact line, so if the player is below the
	// actor the prompt drops with them instead of clamping to eye level and sinking into the ground.
	FVector Direction = Owner->GetActorForwardVector(); // fallback (no tracking / no local camera)

	if (bTrackPlayer)
	{
		if (const APawnController* Controller = GetLocalController())
		{
			FVector ViewLocation;
			FRotator ViewRotation;
			Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);

			const FVector ToView = ViewLocation - Center;
			if (!ToView.IsNearlyZero())
			{
				Direction = ToView.GetSafeNormal();
			}
		}
	}

	// Radius scales with the actor's own size so the prompt clears meshes of any dimension.
	const float Radius = ActorRadius + Margin;

	const FVector NewLocation = Center + Direction * Radius;
	SetWorldLocation(NewLocation);

	if (bDrawDebug)
	{
		DrawDebugSphere(GetWorld(), NewLocation, 15.0f, 12, FColor::Green, false, -1.0f, 0, 1.0f);
	}
}

void UMiniGameConnectionUIComponent::SetShowConnectionUI(bool ShowConnectionUI)
{
	//this->SetHiddenInGame(!ShowConnectionUI); // TODO: this can be optimized and doesnt have to called on every tick
	bShowConnectionUI = ShowConnectionUI;
}