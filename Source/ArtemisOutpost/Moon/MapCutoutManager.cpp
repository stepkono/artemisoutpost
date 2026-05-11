// Fill out your copyright notice in the Description page of Project Settings.


#include "MapCutoutManager.h"

#include <glm/gtx/string_cast.inl>

#include "ArtemisOutpost/AnchorsManagerSubsystem.h"


// Sets default values for this component's properties
UMapCutoutManager::UMapCutoutManager()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UMapCutoutManager::BeginPlay()
{
	Super::BeginPlay();
	
	AnchorsManager = GetWorld()->GetGameInstance()->GetSubsystem<UAnchorsManagerSubsystem>();	
	
	if (AGameStateBase* DefaultGI = GetWorld()->GetGameState())
	{
		GS = Cast<AArtemisGameState>(DefaultGI);
		
		if (!GS)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to cast to ArtemisGameState."));
			return; 
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Unable to get GameState.")); 
		return; 
	}
	
	GS->OnRawAnchorsUpdated.AddDynamic(this, &UMapCutoutManager::HandleAnchorsUpdate);
}


// Called every frame
void UMapCutoutManager::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UMapCutoutManager::HandleAnchorsUpdate(const FCustomAnchors& RawAnchors)
{
	AnchorsManager->DiscoverAnchors(RawAnchors, [this](TArray<AActor*> OutAnchors)
	{
		FAnchorsPositions* RawAnchorsPositions = NewObject<FAnchorsPositions>(); 
		
		RawAnchorsPositions->AAnchorPos = OutAnchors[0]->GetActorLocation();
		RawAnchorsPositions->BAnchorPos = OutAnchors[1]->GetActorLocation();
		RawAnchorsPositions->CAnchorPos = OutAnchors[2]->GetActorLocation();
		RawAnchorsPositions->DAnchorPos = OutAnchors[3]->GetActorLocation();
		
		CalibrateAnchors(RawAnchorsPositions);
	}); 	
}

void UMapCutoutManager::CalibrateAnchors(FAnchorsPositions* RawAnchorsPositions)
{
	
}

