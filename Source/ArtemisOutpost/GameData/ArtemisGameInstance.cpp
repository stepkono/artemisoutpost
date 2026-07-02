// Fill out your copyright notice in the Description page of Project Settings.


#include "ArtemisGameInstance.h"

#include "ArtemisOutpost/Networking/NetUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Persistance/ClientIdentitySave.h"

static const FString IdentitySaveSlot = TEXT("IdentitySaveSlot");

void UArtemisGameInstance::OnStart()
{
	Super::OnStart();
	
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("ArtemisGameInstance: No world found on start."));
		return;
	}
	
	// Only init identity on client
	if (ArtemisNet::IsClientContext(World->GetNetMode()))
	{
		InitializeClientIdentity(); 
	}
}

bool UArtemisGameInstance::CheckForInitializedSpatialAnchors() const
{
	const bool bAnchorsInitialized = RawAnchors.AAnchorUUID.IsValidUUID()
		&& RawAnchors.BAnchorUUID.IsValidUUID()
		&& RawAnchors.CAnchorUUID.IsValidUUID()
		&& RawAnchors.DAnchorUUID.IsValidUUID();
	
	return bAnchorsInitialized; 
}

void UArtemisGameInstance::InitializeClientIdentity()
{
	// Check if the same exists, if so load from save
	if (UGameplayStatics::DoesSaveGameExist(IdentitySaveSlot, 0))
	{
		if (UClientIdentitySave* ClientIdentitySave = Cast<UClientIdentitySave>(UGameplayStatics::LoadGameFromSlot(IdentitySaveSlot, 0)))
		{
			UPID_GI = ClientIdentitySave->GetUPID(); 
			UE_LOG(LogTemp, Log, TEXT("ArtemisGameInstance: Loaded Unique ID from save: %s"), *UPID_GI);
			return; 
		}
	}
	
	// If no save exists
	CreateNewClientIdentity(); 
}

void UArtemisGameInstance::CreateNewClientIdentity()
{
	const FGuid NewGuid = FGuid::NewGuid();
	const FString CachedUPID = NewGuid.ToString();
	
	UClientIdentitySave* ClientIdentitySave = Cast<UClientIdentitySave>(UGameplayStatics::CreateSaveGameObject(UClientIdentitySave::StaticClass()));
	if (ClientIdentitySave)
	{
		ClientIdentitySave->WriteUPID(CachedUPID);
		UGameplayStatics::SaveGameToSlot(ClientIdentitySave, IdentitySaveSlot, 0);
		UPID_GI = ClientIdentitySave->GetUPID();
		
		UE_LOG(LogTemp, Warning, TEXT("ArtemisGameInstance: First time launch. Generated new Unique ID: %s"), *CachedUPID);
	}
}

FString UArtemisGameInstance::GetUPID() const
{
	return UPID_GI;
}