// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ArtemisOutpost/Minigame/MinigameTypes.h"
#include "MinigameLogicComponent.generated.h"

class UConnectionComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMinigameStateChangedBP);

// Abstract rules of a minigame: state machine, start preconditions, and how input mutates
// state. It does NOT manage slots or join/leave — that is the ConnectionComponent's job. It
// reacts to the connection's participant delegates and vetoes joins via a bound predicate.
// Server-authoritative; state replicates to every view; the snapshot hook feeds bridge/logging/
// awareness cues additively (§5, §11, §12).
UCLASS(Abstract, ClassGroup = (Minigame))
class ARTEMISOUTPOST_API UMinigameLogicComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMinigameLogicComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Called on the server by the transport once an input RPC lands. Validates then applies.
	void ServerHandleInput(const FString& UPID, const FMinigameInput& Input);

	UFUNCTION(BlueprintPure, Category = "Minigame")
	EMinigameState GetState() const;

	// Single subscription point for bridge/logging/cues. Payload is a JSON snapshot.
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnSnapshot, const FString& /*Json*/);
	FOnSnapshot OnSnapshotChanged;

	UPROPERTY(BlueprintAssignable, Category = "Minigame")
	FOnMinigameStateChangedBP OnStateChanged;

protected:
	virtual void BeginPlay() override;

	// --- Rules hooks for subclasses (server-side) ---

	// Preconditions for the FIRST participant to start the task (cost/buildability/proximity).
	virtual bool CanStart(const FString& UPID, FText& OutReason) const;
	virtual void OnStart();
	virtual void OnParticipantJoined(const FString& UPID);
	virtual void OnParticipantLeft(const FString& UPID);
	virtual void ApplyInput(const FString& UPID, const FMinigameInput& Input);
	virtual void OnComplete();
	virtual void OnAbort();

	// JSON snapshot for bridge/logging. Base emits state + participants; subclasses call Super.
	virtual TSharedRef<class FJsonObject> BuildSnapshot() const;

	UConnectionComponent* GetConnection() const;
	void SetState(EMinigameState NewState);
	void PublishSnapshot();

	UPROPERTY(ReplicatedUsing = OnRep_State, BlueprintReadOnly, Category = "Minigame")
	EMinigameState State = EMinigameState::Idle;

	UFUNCTION()
	void OnRep_State();

private:
	void HandleStateChanged();

	// Bound to the connection: gate joins on CanStart while Idle, allow otherwise.
	bool HandleCanJoin(const FString& UPID, FText& OutReason);

	// Bound to the connection's participant delegates.
	void HandleParticipantJoined(const FString& UPID);
	void HandleParticipantLeft(const FString& UPID);

	UPROPERTY(Transient)
	UConnectionComponent* CachedConnection = nullptr;
};
