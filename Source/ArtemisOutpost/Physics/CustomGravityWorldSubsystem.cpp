// Fill out your copyright notice in the Description page of Project Settings.


#include "CustomGravityWorldSubSystem.h"
#include "PBDRigidsSolver.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"

void UCustomGravityWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (UWorld* World = GetWorld())
	{
		// Track CMC characters spawned/destroyed at runtime.
		ActorSpawnedHandle   = World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &UCustomGravityWorldSubsystem::AddActorToTrackedCharacters));
		ActorDestroyedHandle = World->AddOnActorDestroyedHandler(FOnActorDestroyed::FDelegate::CreateUObject(this, &UCustomGravityWorldSubsystem::RemoveActorFromTrackedCharacters));
	}
}

void UCustomGravityWorldSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
		World->RemoveOnActorDestroyedHandler(ActorDestroyedHandle);
	}

	TrackedCharacterMovementComponents.Empty();

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

	// Callbacks only fire for actors spawned AFTER this point, so sweep the ones already present.
	for (TActorIterator<AActor> It(&InWorld); It; ++It)
	{
		AddActorToTrackedCharacters(*It);
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

	UpdateCMCGravities();
}

void UCustomGravityWorldSubsystem::UpdateCMCGravities()
{
	for (auto It = TrackedCharacterMovementComponents.CreateIterator(); It; ++It)
	{
		UCharacterMovementComponent* CMComponent = It->Get();
		if (!CMComponent || !CMComponent->GetOwner())
		{
			It.RemoveCurrent();
			continue;
		}

		const FVector CharLocation = CMComponent->GetOwner()->GetActorLocation();

		// Compose Newtonian acceleration from all active attractors (same maths as the Chaos path).
		FVector AdditionalAcceleration = FVector::ZeroVector;
		for (const auto& Attractor : Attractors)
		{
			if (Attractor.IsValid() && Attractor->ApplyGravity)
			{
				const FGravityAttractorData Data = Attractor->GetGravityAttractorData();

				FVector Direction(Data.Location - CharLocation);
				const double SquaredDistance = FVector::DotProduct(Direction, Direction);
				Direction.Normalize();

				const double Intensity = Data.MassDotG / SquaredDistance;
				AdditionalAcceleration += Intensity * Direction;
			}
		}

		// Direction orients the capsule to the surface; force supplies the pull (global gravity is 0).
		// Guard against a zero direction (e.g. attractors not registered yet on the first ticks):
		// SetGravityDirection() ensures !IsNearlyZero, so skip until we have a real direction.
		const FVector GravityDir = AdditionalAcceleration.GetSafeNormal();
		if (!GravityDir.IsNearlyZero())
		{
			CMComponent->SetGravityDirection(GravityDir);
			CMComponent->AddForce(AdditionalAcceleration * CMComponent->Mass);
		}
	}
}

void UCustomGravityWorldSubsystem::AddActorToTrackedCharacters(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	Actor->ForEachComponent<UCharacterMovementComponent>(true, [this](UCharacterMovementComponent* CMComponent)
	{
		if (CMComponent)
		{
			TrackedCharacterMovementComponents.AddUnique(CMComponent);
		}
	});
}

void UCustomGravityWorldSubsystem::RemoveActorFromTrackedCharacters(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	Actor->ForEachComponent<UCharacterMovementComponent>(true, [this](UCharacterMovementComponent* CMComponent)
	{
		if (CMComponent)
		{
			TrackedCharacterMovementComponents.Remove(CMComponent);
		}
	});
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
