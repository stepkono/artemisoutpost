// Fill out your copyright notice in the Description page of Project Settings.

#include "CustomGravityAsyncCallback.h"
#include "PBDRigidsSolver.h"
#include "Chaos/DebugDrawQueue.h"

using namespace Chaos;

double FCustomGravityAsyncCallback::GravitationalConstant = 6.67430E-11;

FCustomGravityAsyncCallback::FCustomGravityAsyncCallback()
{
}

FCustomGravityAsyncCallback::~FCustomGravityAsyncCallback()
{
}

void FCustomGravityAsyncCallback::OnPreSimulate_Internal()
{
	// Here we don't do anything. We just had to register to this Callback to avoid having the "PreIntegrate" filtered.
	// It was a 5.6 workaround, and would not be needed for 5.7.
}

void FCustomGravityAsyncCallback::OnPreIntegrate_Internal()
{
	// Physics thread
	if (FPBDRigidsSolver* PBDSolver = static_cast<FPBDRigidsSolver*>(GetSolver()))
	{	
		// Get a reference to the PT input structure
		const FCustomGravityAsyncInput* Input = GetConsumerInput_Internal();
		if (Input)
		{
			// Iterate over all the currently simulated rigid bodies - They are named Particles in Chaos. 
			TParticleView<FPBDRigidParticles> ActiveParticles = PBDSolver->GetParticles().GetNonDisabledDynamicView();
			
			for (auto& ActiveParticle : ActiveParticles)
			{
				if (ActiveParticle.Handle())
				{
					//UE_LOG(LogTemp, Warning, TEXT("Current active particle: %s"), *ActiveParticle.GetDebugName());
					// Draw current acceleration
					
					// Assuming ActiveParticle.Acceleration() is TVector<double, 3>
					const auto& Accel = ActiveParticle.Acceleration();
					FVector AccelFVec(static_cast<float>(Accel.X), static_cast<float>(Accel.Y), static_cast<float>(Accel.Z));

					// UE_LOG(LogTemp, Warning, TEXT("Particle Acceleration: X=%.3f Y=%.3f Z=%.3f"), AccelFVec.X, AccelFVec.Y, AccelFVec.Z);
					
					FDebugDrawQueue::GetInstance().DrawDebugDirectionalArrow(ActiveParticle.GetX(), ActiveParticle.GetX() + ActiveParticle.Acceleration() * 1000000, 20.f, FColor::Yellow, false, 100, 0, 10000.f);
					
					FVector AdditionalAcceleration = FVector::ZeroVector;
					// Compute the combined forces of all attractors
					for (auto& GravityAttractorData : Input->GravityAttractorsData)
					{
						// TODO: Not sure if this direction is correct
						FVector Direction(GravityAttractorData.Location - ActiveParticle.GetX());
						double SquaredDistance = FVector::DotProduct(Direction, Direction); // We'll be using UE units here, no meters... 
						Direction.Normalize();
 
						// Intensity
						double Intensity = GravityAttractorData.MassDotG / SquaredDistance;
 
						// Add the new acceleration to the force field.  
						AdditionalAcceleration += Intensity * Direction;
						//UE_LOG(LogTemp, Warning, TEXT("Particle Intensity: %f"), static_cast<float>(Intensity));
						//UE_LOG(LogTemp, Warning, TEXT("Particle AdditionalAcceleration: X=%.3f Y=%.3f Z=%.3f"), AdditionalAcceleration.X, AdditionalAcceleration.Y, AdditionalAcceleration.Z);
						//UE_LOG(LogTemp, Warning, TEXT("Particle Direction: X=%.3f Y=%.3f Z=%.3f"), Direction.X, Direction.Y, Direction.Z);
 
						// Debug draw
                        {
							FDebugDrawQueue::GetInstance().DrawDebugLine(ActiveParticle.GetX(), GravityAttractorData.Location, FColor::Magenta, false, 0.5, 0, 1.f);
                        	FDebugDrawQueue::GetInstance().DrawDebugDirectionalArrow(ActiveParticle.GetX(), ActiveParticle.GetX() + Intensity * Direction, 10.f, FColor::White, false, 0.5, 0, 1.f);	
                        	FDebugDrawQueue::GetInstance().DrawDebugString(ActiveParticle.GetX() + Intensity * Direction, * FString::Printf(TEXT("%.2f"), Intensity),nullptr, FColor::White, 0.5, false, 1.0f  );
                        }
					}
 
					{
						FDebugDrawQueue::GetInstance().DrawDebugDirectionalArrow(ActiveParticle.GetX(), ActiveParticle.GetX() + ActiveParticle.Acceleration() + AdditionalAcceleration, 20.f, FColor::Emerald, false, 0.5, 0, 2.f);
						FDebugDrawQueue::GetInstance().DrawDebugDirectionalArrow(ActiveParticle.GetX(), ActiveParticle.GetX() - ActiveParticle.GetR().GetUpVector()* (ActiveParticle.Acceleration() + AdditionalAcceleration).Length(), 20.f, FColor::Cyan, false, 0.5, 0, 1.f);
						FDebugDrawQueue::GetInstance().DrawDebugString(ActiveParticle.GetX(), * FString::Printf(TEXT("I:%.2f / A:%.2f"), ActiveParticle.Acceleration().Length(), AdditionalAcceleration.Length()),nullptr, FColor::Red, 0.5, false, 1.0f  );
					}
					
					// Add the force field value to the rigid body. 
					ActiveParticle.SetAcceleration(ActiveParticle.Acceleration() + AdditionalAcceleration);
				}
			}	
		}
	}
}