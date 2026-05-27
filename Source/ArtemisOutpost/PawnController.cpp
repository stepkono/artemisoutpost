// Fill out your copyright notice in the Description page of Project Settings.


#include "PawnController.h"

void APawnController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	SetViewTarget(InPawn);
/*
	UE_LOG(LogTemp, Warning, TEXT("=== OnPossess ==="));
	UE_LOG(LogTemp, Warning, TEXT("InPawn: %s"), InPawn ? *InPawn->GetName() : TEXT("NULL"));
	UE_LOG(LogTemp, Warning, TEXT("ViewTarget after set: %s"), GetViewTarget() ? *GetViewTarget()->GetName() : TEXT("NULL"));
	*/
}


// CPP
void APawnController::SetViewTarget(AActor* NewViewTarget, 
	FViewTargetTransitionParams TransitionParams)
{
	Super::SetViewTarget(NewViewTarget, TransitionParams);
    
	//UE_LOG(LogTemp, Warning, TEXT("SetViewTarget called: %s"), NewViewTarget ? *NewViewTarget->GetName() : TEXT("NULL"));
    
	// Print callstack to see who is calling it
	//UE_LOG(LogTemp, Warning, TEXT("ViewTarget now: %s"), GetViewTarget() ? *GetViewTarget()->GetName() : TEXT("NULL"));
}