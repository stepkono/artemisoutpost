// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArtemisOutpost/MiniGames/General/Interactable.h"
#include "ArtemisOutpost/MiniGames/MiniGameComponents/PuppetManagerComponent/MinigamePuppetManagerComponent.h"
#include "ArtemisOutpost/MiniGames/UI/UIConnection/MiniGameConnectionUIComponent.h"
#include "ArtemisOutpost/MiniGames/General/MinigameTypes.h"
#include "MinigameActor.generated.h"

class UConnectionComponent;
class UMiniGameUI;
class APawnController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMinigameStateChangedBP);

// Common base for every in-world minigame object (Signal Tower, Habitat, ...). It OWNS the data and
// the state of the game (the replicated State, the participant registry via the ConnectionComponent),
// runs the server-authoritative mechanics (join/start/abort, input handling, completion), and holds
// the RULES itself as overridable virtuals (CanStart + the OnStart/OnComplete/OnAbort reactions) —
// no separate rules component. This actor also feeds the AR puppet and the local screen View from
// its own OnReps (§9).
UCLASS(Abstract)
class ARTEMISOUTPOST_API AMinigameActor : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AMinigameActor();
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// IInteractable
	virtual UConnectionComponent* GetConnectionComponent() const override;

	UFUNCTION(BlueprintPure, Category = "Minigame")
	EMinigameState GetState() const;

	// Persistent, globally-unique id assigned server-side and replicated. Registry key.
	UFUNCTION(BlueprintPure, Category = "Minigame")
	FGuid GetMGID() const { return MGID; }

	// The screen-space View class this minigame opens for a connected local player. Set per game in
	// the actor BP defaults (Signal Tower -> WBP_SignalTowerUI).
	UFUNCTION(BlueprintPure, Category = "Minigame")
	TSubclassOf<UMiniGameUI> GetMiniGameUIClass() const;

	// Server entry point for a validated input intent, called by the transport (the client-owned
	// UMinigamePlayerController). Validates authority + participation, then applies mechanics.
	void ServerHandleInput(const FString& UPID, const FMinigameInput& Input);

	// Model -> View for the local participant's screen UI. One local player, so a delegate is fine.
	UPROPERTY(BlueprintAssignable, Category = "Minigame")
	FOnMinigameStateChangedBP OnStateChanged;

	// Single subscription point for bridge/logging/cues. Payload is a JSON snapshot.
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnSnapshot, const FString& /*Json*/);
	FOnSnapshot OnSnapshotChanged;

	// --- Connect-Prompt read hooks (local) ---

	UFUNCTION(BlueprintPure, Category = "Minigame")
	FString GetLocalPlayerUPID() const;

	UFUNCTION(BlueprintPure, Category = "Minigame")
	bool IsLocalPlayerParticipant() const;

	UFUNCTION(BlueprintPure, Category = "Minigame")
	bool HasFreeSlot() const;

	UFUNCTION(BlueprintPure, Category = "Minigame")
	bool CanLocalPlayerConnect() const;
	
	UFUNCTION()
	TArray<FString> GetActivePlayers(); 

protected:
	virtual void BeginPlay() override;

	// --- Rules + mechanics hooks (server-side); concrete subclasses override ---

	// Precondition for the FIRST participant to start the task (cost/buildability/proximity).
	virtual bool CanStart(const FString& UPID, FText& OutReason) const;

	// Building type for the registry. Concrete subclasses MUST override.
	virtual EOutpostBuildingType GetBuildingType() const
		PURE_VIRTUAL(AMinigameActor::GetBuildingType, return EOutpostBuildingType::SignalTower;);

	// Type-specific registry payload (e.g. a FHabitatData for habitats). Empty by default.
	virtual FInstancedStruct MakeInitialTypeData() const;

	// Lifecycle reactions. Base OnComplete sets State=Completed; subclasses add side effects
	// (score, habitat activation, VFX) and call Super. OnStart/OnAbort are empty by default.
	virtual void OnStart();
	virtual void OnComplete();
	virtual void OnAbort();

	virtual void ApplyInput(const FString& UPID, const FMinigameInput& Input);
	virtual void OnParticipantJoined(const FString& UPID);
	virtual void OnParticipantLeft(const FString& UPID);

	// JSON snapshot for bridge/logging. Base emits state + participants; subclasses call Super.
	virtual TSharedRef<class FJsonObject> BuildSnapshot() const;

	// Pushes the current data to the AR puppet once (initial sync after spawn). Base pushes state;
	// coupled subclasses also push axes.
	virtual void SyncPuppet();

	void SetState(EMinigameState NewState);
	void PublishSnapshot();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minigame UI")
	TSubclassOf<UMiniGameUI> MiniGameUIClass;

	UPROPERTY(ReplicatedUsing = OnRep_State, BlueprintReadOnly, Category = "Minigame")
	EMinigameState State = EMinigameState::Idle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Minigame")
	UConnectionComponent* GameConnection;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Minigame")
	UMinigamePuppetManagerComponent* PuppetManager; 

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minigame UI")
	UMiniGameConnectionUIComponent* ConnectionUIHolder;
	
private:
	UFUNCTION()
	void OnRep_State();
	void HandleStateChanged();

	// Server: builds this actor's registry record and registers it with the buildings manager.
	void RegisterWithBuildingsManager();

	// Server: bound to the connection's join gate + participant delegates.
	bool ServerHandleCanJoin(const FString& UPID, FText& OutReason);
	void ServerHandleParticipantJoined(const FString& UPID);
	void ServerHandleParticipantLeft(const FString& UPID);

	// Client: opens/closes the local player's screen View on membership / state changes.
	UFUNCTION()
	void RefreshLocalUI();
	bool bLocalUIOpen = false;

	bool IsPlayerNear();

	// Local player's controller, cached (re-resolved only if it becomes null).
	APawnController* GetLocalController() const;

	UPROPERTY(Transient)
	mutable APawnController* CachedController = nullptr;

	UPROPERTY()
	TArray<FString> ActivePlayers;

	// Globally-unique id, assigned server-side in BeginPlay, replicated. Registry key.
	UPROPERTY(Replicated)
	FGuid MGID;
};
