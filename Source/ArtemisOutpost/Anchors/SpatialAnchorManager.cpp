// Fill out your copyright notice in the Description page of Project Settings.
/*
#include "SpatialAnchorManager.h"

#include "AnchorSaveGame.h"
#include "SpatialAnchorModel.h"

#include "Kismet/GameplayStatics.h"

// Oculus XR Anchors plugin headers.
// Class names follow the OculusXR plugin shipped with recent UE 5.x releases. If your
// installed plugin uses different paths, adjust these includes (e.g. some versions
// keep async actions under "AsyncActions/" or in a single "OculusXRAnchorActions.h").
#include "OculusXRAnchorBPFunctionLibrary.h"
#include "OculusXRAnchorComponent.h"
#include "OculusXRAnchorTypes.h"
#include "AndroidPermissionFunctionLibrary.h"

// =============================================================================
// Construction / lifecycle
// =============================================================================

USpatialAnchorManager::USpatialAnchorManager()
{
	// Tick is required so the temp / preview model can follow ModelSpawnPositioner.
	PrimaryComponentTick.bCanEverTick = true;
}

void USpatialAnchorManager::BeginPlay()
{
	Super::BeginPlay();
	
	PermissionsGranted = UAndroidPermissionFunctionLibrary::CheckPermission(FString("android.permission.WRITE_EXTERNAL_STORAGE"));
	
	// Request permissions 
	if (!PermissionsGranted)
	{
		TArray<FString> Permissions;
		TArray<FString> Results; 
		
		const FString WritePermission = FString("android.permission.WRITE_EXTERNAL_STORAGE"); 
		const FString ReadPermission = FString("android.permission.READ_EXTERNAL_STORAGE");
	
		Permissions.Add(WritePermission);
		Permissions.Add(ReadPermission);
	
		PermissionsProxy = UAndroidPermissionFunctionLibrary::AcquirePermissions(Permissions);
		if (!PermissionsProxy)
		{
			PermissionsProxy->OnPermissionsGrantedDelegate.AddLambda([this](const TArray<FString>& OutPermission, const TArray<FString>& OutResults)
			{
				for (int32 i = 0; i < OutPermission.Num(); ++i)
				{
					UE_LOG(LogTemp, Warning, TEXT("Permission: %s , Status: %s"), *OutPermission[i], *OutResults[i]);
				}
				
				// Check again if granted
				PermissionsGranted = UAndroidPermissionFunctionLibrary::CheckPermission(FString("android.permission.WRITE_EXTERNAL_STORAGE"));
			});
		}
	}
}

void USpatialAnchorManager::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// "Temp Model Logic" -> Event Tick branch: keep the preview anchor glued to the
	// spawn positioner while it exists.
	if (IsValid(TempSpatialAnchorModel) && IsValid(ModelSpawnPositioner))
	{
		TempSpatialAnchorModel->SetActorTransform(ModelSpawnPositioner->GetComponentTransform());
	}
}

#pragma region CreateAnchor
void USpatialAnchorManager::AnchorCreate()
{
	UWorld* World = GetWorld();
	if (!World || !IsValid(ModelSpawnPositioner) || !SpatialAnchorModelClass)
	{
		return;
	}

	// Spawn spatial anchor model 
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = GetOwner();
	const FTransform SpawnTransform = ModelSpawnPositioner->GetComponentTransform();
	
	ASpatialAnchorModel* SpawnedAnchor = World->SpawnActor<ASpatialAnchorModel>(SpatialAnchorModelClass, SpawnTransform, SpawnParameters);
	if (!SpawnedAnchor)
	{
		return; 
	}
	
	CreateDelegate.BindUObject(this, &USpatialAnchorManager::HandleAnchorCreated, SpawnedAnchor);
	
	// Try create a spatial anchor
	EOculusXRAnchorResult::Type OutResult; 
	OculusXRAnchors::FOculusXRAnchors::CreateSpatialAnchor(SpawnedAnchor->GetTransform(), SpawnedAnchor, CreateDelegate, OutResult); 
	
	if (OutResult != EOculusXRAnchorResult::Type::Success)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to trigger spatial anchor creation."));
	}
}

void USpatialAnchorManager::HandleAnchorCreated(EOculusXRAnchorResult::Type Result, UOculusXRAnchorComponent* AnchorComponent, ASpatialAnchorModel* SpawnedModel)
{
	if (Result != EOculusXRAnchorResult::Type::Success)
	{
		if (IsValid(AnchorComponent))
		{
			SpawnedModel->Destroy();
		}
		return; 
	}

	
	LoadedAnchors.Add(SpawnedModel);
	
	OnAnchorCreated.Broadcast();
}
#pragma endregion

#pragma region SaveAnchor
void USpatialAnchorManager::SaveAnchors(TArray<ASpatialAnchorModel*> SelectedAnchors)
{
	SaveAnchorsDelegate.BindUObject(this, &USpatialAnchorManager::HandleAnchorsSaved); 
	TArray<UOculusXRAnchorComponent*> AnchorComponents;
	
	for (ASpatialAnchorModel* AnchorModel : SelectedAnchors)
	{
		AnchorComponents.Add(AnchorModel->GetSpatialAnchorComponent());
	}
	
			
	EOculusXRAnchorResult::Type OutResult; 
	OculusXRAnchors::FOculusXRAnchors::SaveAnchorList(AnchorComponents, EOculusXRSpaceStorageLocation::Local, SaveAnchorsDelegate, OutResult); 
		
	if (OutResult != EOculusXRAnchorResult::Type::Success)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to trigger spatial anchor saving."));
	}
}

void USpatialAnchorManager::HandleAnchorsSaved(EOculusXRAnchorResult::Type Result, const TArray<UOculusXRAnchorComponent*>& AnchorComponents)
{
	if (Result != EOculusXRAnchorResult::Type::Success)
	{
		for (const auto AnchorComponent : AnchorComponents)
		{
			SavedUUIDs.AddUnique(AnchorComponent->GetUUID());	
		}
	}
	
	SaveGame();
	OnAnchorSaved.Broadcast();
}
#pragma endregion

void USpatialAnchorManager::LoadGame()
{
	// "Load Game (retrieve anchors after an app restart)" graph.
	UOculusXRSpaceDiscoveryIdsFilter* IdsFilter = NewObject<UOculusXRSpaceDiscoveryIdsFilter>(this);
	if (!IdsFilter)
	{
		return;
	}
	IdsFilter->Uuids = SavedUUIDs;

	FOculusXRSpaceDiscoveryInfo DiscoveryInfo;
	DiscoveryInfo.Filters.Add(IdsFilter);

	// OculusXRAnchors::FOculusXRAnchors::DiscoverAnchors()
	
	UOculusXRAsyncActionDiscoverAnchors* Action =
		UOculusXRAsyncActionDiscoverAnchors::OculusXRAsyncDiscoverAnchors(this, DiscoveryInfo);
	if (!Action)
	{
		return;
	}

	Action->Discovered.AddDynamic(this, &USpatialAnchorManager::HandleAnchorsDiscovered);
	Action->Activate();
}

void USpatialAnchorManager::LoadUUIDsFromFile()
{
	// "Load UUIDs From File (populate saved UUIDs from file without actor creation)" graph.
	USaveGame* LoadedSlot = UGameplayStatics::LoadGameFromSlot(SaveGameName, 0);
	UAnchorSaveGame* AnchorSave = Cast<UAnchorSaveGame>(LoadedSlot);
	if (!AnchorSave)
	{
		// Matches the BP development-only Print String node.
		UE_LOG(LogTemp, Display,
			TEXT("[SpatialAnchorManager] Failed to find existing saved Anchors, that's okay we can still save new anchors"));
		return;
	}

	for (const FSavedAnchor& Saved : AnchorSave->SavedAnchors)
	{
		SavedUUIDs.AddUnique(Saved.UUID);
	}
}

void USpatialAnchorManager::SaveGame()
{
	UAnchorSaveGame* Save = Cast<UAnchorSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UAnchorSaveGame::StaticClass()));
	if (!Save)
	{
		return;
	}

	Save->SavedAnchors.Reserve(SavedUUIDs.Num());
	for (const FOculusXRUUID& UUID : SavedUUIDs)
	{
		FSavedAnchor Entry;
		Entry.UUID = UUID;
		Entry.bIsSavedLocal = true;
		Save->SavedAnchors.Add(Entry);
	}

	UGameplayStatics::SaveGameToSlot(Save, SaveGameName, 0);
}

void USpatialAnchorManager::DeleteSaveFile()
{
	// "Delete Save File" graph.
	if (UGameplayStatics::DoesSaveGameExist(SaveGameName, 0))
	{
		UGameplayStatics::DeleteGameInSlot(SaveGameName, 0);
	}
}

// =============================================================================
// Temp / preview model
// =============================================================================

void USpatialAnchorManager::ActivateModel()
{
	// "Temp Model Logic" -> Activate Model.
	// Only spawn a new preview if one isn't already alive.
	if (IsValid(TempSpatialAnchorModel))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World || !IsValid(ModelSpawnPositioner) || !SpatialAnchorModelClass)
	{
		return;
	}

	const FTransform SpawnTransform = ModelSpawnPositioner->GetComponentTransform();

	// Use deferred spawning so we can set the ExposeOnSpawn property bIsTemp before BeginPlay.
	ASpatialAnchorModel* Spawned = World->SpawnActorDeferred<ASpatialAnchorModel>(
		SpatialAnchorModelClass,
		SpawnTransform,
		GetOwner(),
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	if (!Spawned)
	{
		return;
	}

	Spawned->bIsTemp = true;
	UGameplayStatics::FinishSpawningActor(Spawned, SpawnTransform);

	TempSpatialAnchorModel = Spawned;
}

void USpatialAnchorManager::DeactivateModel()
{
	// "Temp Model Logic" -> Deactivate Model.
	if (!IsValid(TempSpatialAnchorModel))
	{
		return;
	}

	TempSpatialAnchorModel->Destroy();
	TempSpatialAnchorModel = nullptr;
}


// =============================================================================
// Discovery (LoadGame result)
// =============================================================================

void USpatialAnchorManager::HandleAnchorsDiscovered(EOculusXRAnchorResult::Type Result, const TArray<FOculusXRAnchorsDiscoverResult>& InAnchors)
{
	CreateAnchorsFromQuery(InAnchors);
}

ASpatialAnchorModel* USpatialAnchorManager::FindAnchor(const FOculusXRUUID& UUID) const
{
	// "Find Anchor" custom function on the manager: returns the loaded model whose
	// UOculusXRAnchorComponent reports the supplied UUID, or nullptr if none match.
	for (const TObjectPtr<ASpatialAnchorModel>& Model : LoadedAnchors)
	{
		if (!IsValid(Model))
		{
			continue;
		}

		const UOculusXRAnchorComponent* AnchorComp = Model->FindComponentByClass<UOculusXRAnchorComponent>();
		if (AnchorComp && AnchorComp->GetUUID() == UUID)
		{
			return Model;
		}
	}
	return nullptr;
}

void USpatialAnchorManager::CreateAnchorsFromQuery(const TArray<FOculusXRAnchorsDiscoverResult>& Anchors)
{
	// "CreateAnchorsFromQuery" graph.
	if (!SpatialAnchorModelClass)
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	APawn* InstigatorPawn = OwnerActor ? OwnerActor->GetInstigator() : nullptr;

	for (const FOculusXRAnchorsDiscoverResult& Result : Anchors)
	{
		// Skip results we've already loaded into the world.
		if (FindAnchor(Result.UUID) != nullptr)
		{
			continue;
		}

		bool bSpawnSuccess = false;
		AActor* SpawnedActor = UOculusXRAnchorBPFunctionLibrary::SpawnOculusAnchorActor(
			this,
			Result.Space,
			Result.UUID,
			EOculusXRAnchorLocationFlags::Local,
			SpatialAnchorModelClass,
			OwnerActor,
			InstigatorPawn,
			ESpawnActorCollisionHandlingMethod::Default,
			bSpawnSuccess);

		ASpatialAnchorModel* Model = Cast<ASpatialAnchorModel>(SpawnedActor);

		if (bSpawnSuccess && IsValid(Model))
		{
			Model->AddContextInfo();
			LoadedAnchors.AddUnique(Model);
			OnAnchorLoaded.Broadcast();
		}
		else if (IsValid(SpawnedActor))
		{
			// Spawn reported failure – clean up the actor so we don't leak it.
			SpawnedActor->Destroy();
		}
	}
}


void USpatialAnchorManager::DestroyAnchors()
{
	// "Destroy Anchors (Delete Actor and Unsave)" graph.
	ToEraseAnchors = SelectedAnchors;
	SelectedAnchors.Reset();

	TArray<AActor*> TargetActors;
	TargetActors.Reserve(ToEraseAnchors.Num());
	for (ASpatialAnchorModel* Model : ToEraseAnchors)
	{
		if (IsValid(Model))
		{
			TargetActors.Add(Model);
		}
	}

	if (TargetActors.Num() == 0)
	{
		return;
	}

	UOculusXRAsyncActionEraseAnchors* Action =
		UOculusXRAsyncActionEraseAnchors::OculusXRAsyncEraseAnchors(this, TargetActors);
	if (!Action)
	{
		return;
	}

	Action->Success.AddDynamic(this, &USpatialAnchorManager::HandleAnchorsErasedForDestroy);
	Action->Activate();
}

void USpatialAnchorManager::HandleAnchorsErasedForDestroy(EOculusXRAnchorResult::Type Result, const TArray<FOculusXRUUID>& InAnchorUUIDs)
{
	// We have the actor list locally in ToEraseAnchors – iterate that directly, it makes
	// the LoadedAnchors / SavedUUIDs cleanup straightforward.
	for (ASpatialAnchorModel* Model : ToEraseAnchors)
	{
		if (!IsValid(Model))
		{
			continue;
		}

		LoadedAnchors.Remove(Model);

		if (UOculusXRAnchorComponent* AnchorComp = Model->FindComponentByClass<UOculusXRAnchorComponent>())
		{
			SavedUUIDs.Remove(AnchorComp->GetUUID());
		}

		Model->RemoveContextInfo();
		Model->Destroy();
	}

	ToEraseAnchors.Reset();
	SaveGame();
}

// =============================================================================
// Unsave anchors -> erase from disk but keep the actor in the level
// =============================================================================

void USpatialAnchorManager::UnsaveAnchors()
{
	// "Unsave Anchors (Remove from device storage)" graph.
	// Refresh SavedUUIDs from disk first so we're operating on the authoritative list.
	LoadUUIDsFromFile();

	TArray<AActor*> TargetActors;
	TargetActors.Reserve(SelectedAnchors.Num());
	for (ASpatialAnchorModel* Model : SelectedAnchors)
	{
		if (IsValid(Model))
		{
			TargetActors.Add(Model);
		}
	}

	if (TargetActors.Num() == 0)
	{
		return;
	}

	UOculusXRAsyncActionEraseAnchors* Action =
		UOculusXRAsyncActionEraseAnchors::OculusXRAsyncEraseAnchors(this, TargetActors);
	if (!Action)
	{
		return;
	}

	Action->Success.AddDynamic(this, &USpatialAnchorManager::HandleAnchorsErasedForUnsave);
	Action->Activate();
}

void USpatialAnchorManager::HandleAnchorsErasedForUnsave(EOculusXRAnchorResult::Type Result, const TArray<FOculusXRUUID>& InAnchorUUIDs)
{
	// First loop in BP: drop each erased UUID from the cache.
	for (const FOculusXRUUID& UUID : InAnchorUUIDs)
	{
		SavedUUIDs.Remove(UUID);
	}

	// Second loop in BP: tear down the floating context info on each unsaved model
	// (the actor itself stays in the level, unlike DestroyAnchors).
	for (ASpatialAnchorModel* Model : SelectedAnchors)
	{
		if (IsValid(Model))
		{
			Model->RemoveContextInfo();
		}
	}

	SaveGame();
}
*/