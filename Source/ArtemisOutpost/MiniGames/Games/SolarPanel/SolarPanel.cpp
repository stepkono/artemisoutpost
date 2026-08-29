// Fill out your copyright notice in the Description page of Project Settings.


#include "SolarPanel.h"


// Sets default values
ASolarPanel::ASolarPanel()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	MiniGameType = EMiniGameType::SolarPanel; 
}

// Called when the game starts or when spawned
void ASolarPanel::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ASolarPanel::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

