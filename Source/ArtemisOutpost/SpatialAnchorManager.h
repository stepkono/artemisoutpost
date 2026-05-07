/*
// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OculusXRAnchorTypes.h"
#include "AndroidPermissionCallbackProxy.h"
#include "OculusXRAnchors.h"
#include "SpatialAnchorModel.h"
#include "SpatialAnchorManager.generated.h"

class UOculusXRAnchorComponent;

/** Simple multicast dispatcher for "anchor created" / "anchor saved" events. #1#
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAnchorManagerSimpleDispatch);

/**
 * Component that manages the full lifecycle of Oculus spatial anchors:
 *  - discovering anchors persisted on device on startup
 *  - creating new anchors at a "spawn positioner" transform
 *  - saving / unsaving anchors to local device storage
 *  - destroying anchors and their backing actors
 *
 * This is the C++ migration of BP_SpatialAnchorManagerComponent from the Meta
 * Spatial Anchors sample. The accompanying ASpatialAnchorModel actor represents
 * a single visualised anchor in the world.
 #1#
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class ARTEMISOUTPOST_API USpatialAnchorManager : public UActorComponent
{
	GENERATED_BODY()

public:
	USpatialAnchorManager();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

	// =====================================================================
	// State
	// =====================================================================
public:
	/** External scene component used as the spawn / preview transform for new anchors. #1#
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components")
	TObjectPtr<USceneComponent> ModelSpawnPositioner;

	// ----- Selection -----

	/** Transient preview model that follows ModelSpawnPositioner before an anchor is created. #1#
	UPROPERTY(BlueprintReadWrite, Category = "Selection")
	TObjectPtr<ASpatialAnchorModel> TempSpatialAnchorModel;

	/** Anchor currently under the user's reticle / cursor. #1#
	UPROPERTY(BlueprintReadWrite, Category = "Selection")
	TObjectPtr<ASpatialAnchorModel> HoveredAnchor;

	UPROPERTY(BlueprintReadWrite, Category = "Selection")
	bool bSelectionMode = false;

	UPROPERTY(BlueprintReadWrite, Category = "Anchor Containers")
	TArray<TObjectPtr<ASpatialAnchorModel>> SelectedAnchors;

	UPROPERTY(BlueprintReadWrite, Category = "Anchor Containers")
	TArray<TObjectPtr<ASpatialAnchorModel>> PreviewAnchors;

	UPROPERTY(BlueprintReadWrite, Category = "Anchor Containers")
	TArray<TObjectPtr<ASpatialAnchorModel>> ToEraseAnchors;

	// ----- Settings -----

	/** Maximum number of anchors that may be created/loaded at once. #1#
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	int32 SpatialAnchorLimit = 100;

	/** Whether preview anchors should be loaded alongside saved ones. #1#
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	bool bLoadPreview = true;

	/** Class spawned to represent any single anchor in the world. #1#
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	TSubclassOf<ASpatialAnchorModel> SpatialAnchorModelClass;

	// ----- Save game -----

	/** UUIDs that are currently considered "saved" on the device. #1#
	UPROPERTY(BlueprintReadWrite, Category = "Save Game")
	TArray<FOculusXRUUID> SavedUUIDs;

	/** Slot name for the UAnchorSaveGame on disk. #1#
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save Game")
	FString SaveGameName = TEXT("AnchorSaveGame");

	// ----- Messages -----

	UPROPERTY(BlueprintReadWrite, Category = "Messages")
	bool bAnchorCreationError = false;

	// ----- Dispatchers -----

	UPROPERTY(BlueprintAssignable, Category = "SpatialAnchorManager|Events")
	FAnchorManagerSimpleDispatch OnAnchorCreated;

	UPROPERTY(BlueprintAssignable, Category = "SpatialAnchorManager|Events")
	FAnchorManagerSimpleDispatch OnAnchorSaved;

	UPROPERTY(BlueprintAssignable, Category = "SpatialAnchorManager|Events")
	FAnchorManagerSimpleDispatch OnAnchorLoaded;
	
	UPROPERTY(BlueprintReadOnly, Category = "Anchor Containers")
	TArray<TObjectPtr<ASpatialAnchorModel>> LoadedAnchors;
	

public: 
	/** Discover anchors previously saved to device using the cached SavedUUIDs. #1#
	UFUNCTION(BlueprintCallable, Category = "SpatialAnchorManager|Save")
	void LoadGame();

	/** Read SavedUUIDs from the persistent UAnchorSaveGame slot without spawning actors. #1#
	UFUNCTION(BlueprintCallable, Category = "SpatialAnchorManager|Save")
	void LoadUUIDsFromFile();

	/** Persist the current SavedUUIDs list to the UAnchorSaveGame slot. #1#
	UFUNCTION(BlueprintCallable, Category = "SpatialAnchorManager|Save")
	void SaveGame();

	/** Delete the on-disk save game slot if it exists. #1#
	UFUNCTION(BlueprintCallable, Category = "SpatialAnchorManager|Save")
	void DeleteSaveFile();

	/** Spawn the transient preview model that follows ModelSpawnPositioner. #1#
	UFUNCTION(BlueprintCallable, Category = "SpatialAnchorManager|Model")
	void ActivateModel();

	/** Destroy the transient preview model. #1#
	UFUNCTION(BlueprintCallable, Category = "SpatialAnchorManager|Model")
	void DeactivateModel();

	/** Spawn a new ASpatialAnchorModel and create a backing Oculus spatial anchor at the spawn transform. #1#
	UFUNCTION(BlueprintCallable, Category = "SpatialAnchorManager|Anchors")
	void AnchorCreate();

	/** Persist all SelectedAnchors to local device storage. #1#
	UFUNCTION(BlueprintCallable, Category = "SpatialAnchorManager|Anchors")
	void SaveAnchors(TArray<ASpatialAnchorModel*> SelectedAnchors);

	/** Erase SelectedAnchors from device storage AND destroy the actors. #1#
	UFUNCTION(BlueprintCallable, Category = "SpatialAnchorManager|Anchors")
	void DestroyAnchors();

	/** Erase SelectedAnchors from device storage but leave the actors in the level. #1#
	UFUNCTION(BlueprintCallable, Category = "SpatialAnchorManager|Anchors")
	void UnsaveAnchors();

	/**
	 * Spawn ASpatialAnchorModel actors for each result returned by an Oculus discovery query
	 * and add them to LoadedAnchors. Skips any UUID that already has a matching loaded anchor
	 * (see FindAnchor).
	 #1#
	UFUNCTION(BlueprintCallable, Category = "SpatialAnchorManager|Anchors")
	void CreateAnchorsFromQuery(const TArray<FOculusXRAnchorsDiscoverResult>& Anchors);

	/**
	 * Look up an already-loaded ASpatialAnchorModel by its Oculus UUID.
	 * Returns nullptr if no matching anchor is currently in LoadedAnchors.
	 #1#
	UFUNCTION(BlueprintCallable, Category = "SpatialAnchorManager|Anchors")
	ASpatialAnchorModel* FindAnchor(const FOculusXRUUID& UUID) const;

	// =====================================================================
	// Internal async-action callbacks (UFUNCTION because they bind to dynamic delegates)
	// =====================================================================
private:
#pragma region Handlers
	UFUNCTION()
	void HandleAnchorsDiscovered(EOculusXRAnchorResult::Type Result, const TArray<FOculusXRAnchorsDiscoverResult>& InAnchors);

	void HandleAnchorCreated(EOculusXRAnchorResult::Type Result, UOculusXRAnchorComponent* AnchorComponent, ASpatialAnchorModel* AssociatedModel);

	UFUNCTION()
	void HandleAnchorsSaved(EOculusXRAnchorResult::Type Result, const TArray<UOculusXRAnchorComponent*>& AnchorComponents);

	UFUNCTION()
	void HandleAnchorsErasedForDestroy(EOculusXRAnchorResult::Type Result, const TArray<FOculusXRUUID>& InAnchorUUIDs);

	UFUNCTION()
	void HandleAnchorsErasedForUnsave(EOculusXRAnchorResult::Type Result, const TArray<FOculusXRUUID>& InAnchorUUIDs);
#pragma endregion	
	
	/** Single-cast delegate fired by FOculusXRAnchors::CreateSpatialAnchor when the async create finishes. #1#
	FOculusXRSpatialAnchorCreateDelegate CreateDelegate;

	/**
	 * The actor we've just spawned for AnchorCreate; held while the async create call is in flight
	 * so the result handler can either commit it to LoadedAnchors or destroy it.
	 #1#
	UPROPERTY()
	TObjectPtr<ASpatialAnchorModel> PendingCreateModel;
	
#pragma region Callbacks
	UPROPERTY()
	UAndroidPermissionCallbackProxy* PermissionsProxy; 
	
	UPROPERTY()
	FOculusXRAnchorSaveListDelegate& SaveAnchorsDelegate; 
#pragma endregion
	
	UPROPERTY()
	bool PermissionsGranted; 
};
*/
