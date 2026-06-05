// Fill out your copyright notice in the Description page of Project Settings.


#include "CustomGravityWorldSubSystem.h"
#include "PBDRigidsSolver.h"
#include "Physics/Experimental/PhysScene_Chaos.h"

void UCustomGravityWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UCustomGravityWorldSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UCustomGravityWorldSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	
	GEngine->AddOnScreenDebugMessage( -1, 10, FColor::Green, *FString::Printf(TEXT("%d Attractors in the World"), Attractors.Num()));
	
	if (Attractors.Num() > 0 && Attractors[0]->GetOwner())
	{
		const FString AttractorName = Attractors[0]->GetOwner()->GetName();
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Green, FString::Printf(TEXT("Name of the attractor: %s"), *AttractorName));
	}
	
	if (!IsAsyncCallbackRegistered())
	{
		RegisterAsyncCallback();
	}
}

void UCustomGravityWorldSubsystem::RegisterAsyncCallback()
{
	if (const UWorld* World = GetWorld())
	{
		if (const FPhysScene* PhysScene = World->GetPhysicsScene())
		{
			AsyncCallback = PhysScene->GetSolver()->CreateAndRegisterSimCallbackObject_External<FCustomGravityAsyncCallback>();
		}
	}
}

bool UCustomGravityWorldSubsystem::IsAsyncCallbackRegistered() const
{
	return AsyncCallback != nullptr;
}

void UCustomGravityWorldSubsystem::AddGravityAttractorData(const FGravityAttractorData& InputData) const
{
	if (IsAsyncCallbackRegistered())
	{
		FCustomGravityAsyncInput* Input = AsyncCallback->GetProducerInputData_External();
		
		Input->GravityAttractorsData.Add(InputData);
	}
}

TStatId UCustomGravityWorldSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCustomGravityWorldSubSystem, STATGROUP_Tickables);
}

void UCustomGravityWorldSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

//// 

void UCustomGravityWorldSubsystem::AddAttractor(UGravityAttractorComponent* GravityAttractorComponent)
{
	Attractors.Add(GravityAttractorComponent);
}

void UCustomGravityWorldSubsystem::RemoveAttractor(UGravityAttractorComponent* GravityAttractorComponent)
{
	
	Attractors.Remove(GravityAttractorComponent);
}
