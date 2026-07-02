// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"            // ETravelFailure
#include "Net/Core/Connection/NetEnums.h"      // ENetworkFailure
#include "Subsystems/GameInstanceSubsystem.h"
#include "ConnectionLifecycleSubsystem.generated.h"

class UNetDriver;

/**
 * One place that observes the client/server connection lifecycle across level travel:
 *  - engine network/travel failures (client sees ConnectionLost/Timeout here; this is what sends a
 *    client back to the default map),
 *  - server player join/leave via FGameModeEvents (Logout also fires on a timeout-drop),
 *  - app suspend/resume (on Quest, taking the HMD off pauses the app — the suspected disconnect trigger),
 *  - map loads (to see the return to SA_Showcase).
 *
 * [Basket B] Currently instrumentation-only (logs tagged [NetLife]); this is also the intended home
 * for client-triggered reconnect logic, since a GameInstanceSubsystem outlives the GameMode/pawns.
 */
UCLASS()
class ARTEMISOUTPOST_API UConnectionLifecycleSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);
	void HandlePostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer);
	void HandleLogout(AGameModeBase* GameMode, AController* Exiting);
	void HandlePostLoadMap(UWorld* LoadedWorld);
	void HandleAppWillEnterBackground();
	void HandleAppHasEnteredForeground();
	void HandleAppWillDeactivate();
	void HandleAppHasReactivated();

	FString NetModeString() const;

	// ---- Client reconnect ----
	// The Quest suspends on HMD doff; after the server's ConnectionTimeout it drops the client, and on
	// re-don the native "restart handshake" can't resume a connection the server already discarded — so
	// we drive an explicit fresh rejoin. Lives here (GameInstance-scoped) because it must survive the
	// bounce to SA_Showcase. Runs on clients only.
	bool IsClientContextNow() const;
	void CaptureServerURLIfConnected(UWorld* World);
	FString BuildReconnectURL() const;
	void BeginReconnect(const TCHAR* Reason);  // reset attempts, try immediately
	void ScheduleReconnect();                  // (re)arm the backoff timer
	void TryReconnect();

private:
	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle TravelFailureHandle;
	FDelegateHandle PostLoginHandle;
	FDelegateHandle LogoutHandle;
	FDelegateHandle PostLoadMapHandle;
	FDelegateHandle AppBackgroundHandle;
	FDelegateHandle AppForegroundHandle;
	FDelegateHandle AppDeactivateHandle;
	FDelegateHandle AppReactivateHandle;

	// Reconnect state.
	FString LastServerURL;                 // "host:port" captured while connected
	bool bHasBeenConnected = false;        // we successfully joined the server at least once
	bool bReconnecting = false;            // a reconnect sequence is in progress
	int32 ReconnectAttempt = 0;
	FTimerHandle ReconnectTimerHandle;
	double BackgroundedAtUnix = -1.0;      // wall-clock at suspend, to measure doff duration

	// Tuning.
	static constexpr float ReconnectBaseDelaySeconds = 1.5f;
	static constexpr float ReconnectMaxDelaySeconds = 10.0f;
	static constexpr int32 MaxReconnectAttempts = 100;             // exhibit: keep trying a long time
	static constexpr float AssumeDroppedAfterBackgroundSeconds = 45.0f; // doff longer than this => server dropped us
};
