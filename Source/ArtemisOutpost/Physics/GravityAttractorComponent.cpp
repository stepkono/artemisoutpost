// Fill out your copyright notice in the Description page of Project Settings.


#include "GravityAttractorComponent.h"
#include "CustomGravityWorldSubsystem.h"

// Sets default values for this component's properties
UGravityAttractorComponent::UGravityAttractorComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// Important, we need to be ticked before the Physics Thread to send the attractor location! 
	PrimaryComponentTick.bStartWithTickEnabled = true; 
	PrimaryComponentTick.TickGroup = ETickingGroup::TG_PrePhysics; 
}

FGravityAttractorData UGravityAttractorComponent::GetGravityAttractorData() const
{
	FGravityAttractorData GravityAttractorData;
	GravityAttractorData.Location = GetComponentLocation();

	if (bUseGravityAtRadius)
	{
		GravityAttractorData.MassDotG = Gravity * Radius*Radius;
	}
	else
	{
		GravityAttractorData.MassDotG = Mass * 6.67430E-5 ; // G = 6.67430E-11 m³kg⁻¹s⁻², because 1m equals 100 UE Units, we have to multiply by a 100³ factor, so E-11 goes E-5 
	}
	return GravityAttractorData;
}

void UGravityAttractorComponent::BuildAsyncInput()
{
	if (ApplyGravity) // Publish attractor data only if active
	{
		if (const UWorld* World = GetWorld())
		{
			if (const UCustomGravityWorldSubsystem* GravitySubsystem = World->GetSubsystem<UCustomGravityWorldSubsystem>())
			{
				GravitySubsystem->AddGravityAttractorData(GetGravityAttractorData());
			}
		}	
	}
}

// Called every frame
void UGravityAttractorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	BuildAsyncInput();
}

void UGravityAttractorComponent::OnRegister()
{
	Super::OnRegister();
	
	if (const UWorld* World = GetWorld())
	{
		if (UCustomGravityWorldSubsystem* GravityWorldSubSystem = World->GetSubsystem<UCustomGravityWorldSubsystem>())
		{
			GravityWorldSubSystem->AddAttractor(this);
		}
	}
}

void UGravityAttractorComponent::OnUnregister()
{
	if (const UWorld* World = GetWorld())
    {
    	if (UCustomGravityWorldSubsystem* GravitySubsystem = World->GetSubsystem<UCustomGravityWorldSubsystem>())
    	{
    		GravitySubsystem->RemoveAttractor(this);
    	}
    }
    	
	Super::OnUnregister();
}


// Called when the game starts
void UGravityAttractorComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}
