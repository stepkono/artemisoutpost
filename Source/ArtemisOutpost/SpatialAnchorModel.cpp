// Fill out your copyright notice in the Description page of Project Settings.


#include "SpatialAnchorModel.h"

#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "UObject/FastReferenceCollector.h"

// Sets default values
ASpatialAnchorModel::ASpatialAnchorModel()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void ASpatialAnchorModel::BeginPlay()
{
	Super::BeginPlay();

}

void ASpatialAnchorModel::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Cache the initial Z offset of the context-info point relative to the actor so
	// UpdateContextInfoLocation() can keep it floating at the correct height.
	if (ContextInfoPoint)
	{
		ContextInfoPointOffsetZ = ContextInfoPoint->GetRelativeLocation().Z;
	}

	// TODO: Construction-time material setup (creating dynamic instances for
	// AnchorIcon / AnchorBG and applying them to AnchorBackground) will be
	// migrated when BP_SpatialAnchorModel is fully ported to C++.
	UpdateColorFeedback();
}

// Called every frame
void ASpatialAnchorModel::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!ContextInfoPoint)
	{
		return;
	}

	const FRotator RotToCam = GetRotationToCamera(ContextInfoPoint->GetComponentLocation());
	ContextInfoPoint->SetWorldRotation(RotToCam);
	UpdateContextInfoLocation();
}

FRotator ASpatialAnchorModel::GetRotationToCamera(const FVector &WorldPosition) const
{
	const FVector CurrentCamPosition = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0)->GetCameraLocation();
	return UKismetMathLibrary::MakeRotFromX(CurrentCamPosition);
}

void ASpatialAnchorModel::UpdateContextInfoLocation() const
{
	FVector NewWorldLocation = this->GetActorLocation();
	NewWorldLocation.Z = this->GetActorLocation().Z + ContextInfoPointOffsetZ;
	ContextInfoPoint->SetWorldLocation(NewWorldLocation);
}

void ASpatialAnchorModel::AddContextInfo()
{
	// TODO: Implementation pending migration of the BP_SpatialAnchorModel "Add Context Info" event.
	// Called by USpatialAnchorManager once an anchor has been successfully saved to device storage.
}

void ASpatialAnchorModel::RemoveContextInfo()
{
	// TODO: Implementation pending migration of the BP_SpatialAnchorModel "Remove Context Info" event.
	// Called by USpatialAnchorManager just before the anchor actor is destroyed or unsaved.
}

void ASpatialAnchorModel::UpdateColorFeedback()
{
	// TODO: Implementation pending migration of the BP_SpatialAnchorModel "Update Color Feedback" event.
	// Updates dynamic material parameters on AnchorIcon / AnchorBG to reflect selection state.
}

UOculusXRAnchorComponent* ASpatialAnchorModel::GetSpatialAnchorComponent()
{
	return SpatialAnchorComponent;
}
