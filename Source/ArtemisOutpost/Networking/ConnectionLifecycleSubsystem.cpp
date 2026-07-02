// Fill out your copyright notice in the Description page of Project Settings.


#include "ConnectionLifecycleSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/NetDriver.h"
#include "Engine/NetConnection.h"
#include "TimerManager.h"
#include "Misc/CoreDelegates.h"
#include "Misc/DateTime.h"
#include "UObject/UObjectGlobals.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "ArtemisOutpost/Networking/NetUtils.h"
#include "ArtemisOutpost/GameData/ArtemisGameInstance.h"

void UConnectionLifecycleSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (GEngine)
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &UConnectionLifecycleSubsystem::HandleNetworkFailure);
		TravelFailureHandle  = GEngine->OnTravelFailure().AddUObject(this, &UConnectionLifecycleSubsystem::HandleTravelFailure);
	}

	// Server-only signals (a GameMode only exists on the authority): player join, and leave/timeout-drop.
	PostLoginHandle = FGameModeEvents::GameModePostLoginEvent.AddUObject(this, &UConnectionLifecycleSubsystem::HandlePostLogin);
	LogoutHandle    = FGameModeEvents::GameModeLogoutEvent.AddUObject(this, &UConnectionLifecycleSubsystem::HandleLogout);

	// Shows the return to the default map (SA_Showcase) after a client drops.
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UConnectionLifecycleSubsystem::HandlePostLoadMap);

	// App lifecycle — on Quest, taking the HMD off pauses the app (suspected disconnect trigger).
	AppBackgroundHandle = FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddUObject(this, &UConnectionLifecycleSubsystem::HandleAppWillEnterBackground);
	AppForegroundHandle = FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddUObject(this, &UConnectionLifecycleSubsystem::HandleAppHasEnteredForeground);
	AppDeactivateHandle = FCoreDelegates::ApplicationWillDeactivateDelegate.AddUObject(this, &UConnectionLifecycleSubsystem::HandleAppWillDeactivate);
	AppReactivateHandle = FCoreDelegates::ApplicationHasReactivatedDelegate.AddUObject(this, &UConnectionLifecycleSubsystem::HandleAppHasReactivated);

	UE_LOG(LogTemp, Warning, TEXT("[NetLife][%s] Subsystem initialized — watching network/travel failures, player join/leave, app lifecycle, map loads."), *NetModeString());
}

void UConnectionLifecycleSubsystem::Deinitialize()
{
	if (GEngine)
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
	}
	FGameModeEvents::GameModePostLoginEvent.Remove(PostLoginHandle);
	FGameModeEvents::GameModeLogoutEvent.Remove(LogoutHandle);
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Remove(AppBackgroundHandle);
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Remove(AppForegroundHandle);
	FCoreDelegates::ApplicationWillDeactivateDelegate.Remove(AppDeactivateHandle);
	FCoreDelegates::ApplicationHasReactivatedDelegate.Remove(AppReactivateHandle);

	if (GetGameInstance())
	{
		GetGameInstance()->GetTimerManager().ClearTimer(ReconnectTimerHandle);
	}

	Super::Deinitialize();
}

void UConnectionLifecycleSubsystem::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	// On a client this is the disconnect that (by default) sends us back to the default map.
	UE_LOG(LogTemp, Error, TEXT("[NetLife][%s] NetworkFailure=%s | reason='%s' | NetDriver=%s"),
		*NetModeString(), ENetworkFailure::ToString(FailureType), *ErrorString, *GetNameSafe(NetDriver));

	// Client lost the connection → start (or continue) reconnecting. Safety net for real drops while
	// worn and for cases the resume heuristic misses.
	if (IsClientContextNow() && bHasBeenConnected)
	{
		if (!bReconnecting)
		{
			bReconnecting = true;
			ReconnectAttempt = 0;
		}
		ScheduleReconnect();
	}
}

void UConnectionLifecycleSubsystem::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	UE_LOG(LogTemp, Error, TEXT("[NetLife][%s] TravelFailure=%s | reason='%s'"),
		*NetModeString(), ETravelFailure::ToString(FailureType), *ErrorString);
}

void UConnectionLifecycleSubsystem::HandlePostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer)
{
	UE_LOG(LogTemp, Warning, TEXT("[NetLife][%s] PostLogin: %s"), *NetModeString(), *GetNameSafe(NewPlayer));
}

void UConnectionLifecycleSubsystem::HandleLogout(AGameModeBase* GameMode, AController* Exiting)
{
	// Fires on a graceful leave AND on a timeout-drop — the server-side view of the disconnect.
	UE_LOG(LogTemp, Warning, TEXT("[NetLife][%s] Logout: %s"), *NetModeString(), *GetNameSafe(Exiting));
}

void UConnectionLifecycleSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	UE_LOG(LogTemp, Warning, TEXT("[NetLife][%s] Loaded map: %s"),
		*NetModeString(), LoadedWorld ? *LoadedWorld->GetName() : TEXT("?"));

	if (!IsClientContextNow())
	{
		return; // server: nothing to reconnect
	}

	// Are we now actually connected to a server (initial join or successful reconnect)?
	const bool bConnected = LoadedWorld && LoadedWorld->GetNetDriver() && LoadedWorld->GetNetDriver()->ServerConnection != nullptr;
	if (bConnected)
	{
		CaptureServerURLIfConnected(LoadedWorld);
		bReconnecting = false;
		ReconnectAttempt = 0;
		if (GetGameInstance())
		{
			GetGameInstance()->GetTimerManager().ClearTimer(ReconnectTimerHandle);
		}
		UE_LOG(LogTemp, Warning, TEXT("[NetLife][CLIENT] Connected to server (%s). Reconnect state cleared."), *LastServerURL);
	}
	else if (bHasBeenConnected)
	{
		// We were connected before but landed on a local map (e.g. bounced to SA_Showcase) → keep trying.
		UE_LOG(LogTemp, Warning, TEXT("[NetLife][CLIENT] On a local map but expected the server — scheduling reconnect."));
		ScheduleReconnect();
	}
}

void UConnectionLifecycleSubsystem::HandleAppWillEnterBackground()
{
	UE_LOG(LogTemp, Warning, TEXT("[NetLife][%s] App WILL ENTER BACKGROUND (HMD doff / suspend)."), *NetModeString());
	BackgroundedAtUnix = static_cast<double>(FDateTime::UtcNow().ToUnixTimestamp());
}

void UConnectionLifecycleSubsystem::HandleAppHasEnteredForeground()
{
	UE_LOG(LogTemp, Warning, TEXT("[NetLife][%s] App HAS ENTERED FOREGROUND (HMD re-don / resume)."), *NetModeString());

	if (!IsClientContextNow() || !bHasBeenConnected)
	{
		BackgroundedAtUnix = -1.0;
		return;
	}

	// If the doff outlasted the server's ConnectionTimeout, the server has already dropped us and the
	// native restart-handshake can't resume — force a fresh rejoin immediately instead of waiting out
	// the client's own ~60s timeout (which is what dumps us to SA_Showcase). Short doffs are left to
	// the native resume so we don't churn a still-live connection.
	const double DoffSeconds = (BackgroundedAtUnix > 0.0)
		? static_cast<double>(FDateTime::UtcNow().ToUnixTimestamp()) - BackgroundedAtUnix
		: -1.0;
	BackgroundedAtUnix = -1.0;

	if (DoffSeconds < 0.0 || DoffSeconds >= AssumeDroppedAfterBackgroundSeconds)
	{
		UE_LOG(LogTemp, Warning, TEXT("[NetLife][CLIENT] Doff ~%.0fs (>= %.0fs) — forcing fresh reconnect."),
			DoffSeconds, AssumeDroppedAfterBackgroundSeconds);
		BeginReconnect(TEXT("resume-after-long-doff"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[NetLife][CLIENT] Doff ~%.0fs (< %.0fs) — trusting native resume."),
			DoffSeconds, AssumeDroppedAfterBackgroundSeconds);
	}
}

void UConnectionLifecycleSubsystem::HandleAppWillDeactivate()
{
	UE_LOG(LogTemp, Warning, TEXT("[NetLife][%s] App WILL DEACTIVATE."), *NetModeString());
}

void UConnectionLifecycleSubsystem::HandleAppHasReactivated()
{
	UE_LOG(LogTemp, Warning, TEXT("[NetLife][%s] App HAS REACTIVATED."), *NetModeString());
}

bool UConnectionLifecycleSubsystem::IsClientContextNow() const
{
	const UWorld* W = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	return W && ArtemisNet::IsClientContext(W->GetNetMode());
}

void UConnectionLifecycleSubsystem::CaptureServerURLIfConnected(UWorld* World)
{
	if (!World)
	{
		return;
	}
	if (const UNetDriver* ND = World->GetNetDriver())
	{
		if (ND->ServerConnection)
		{
			const FURL& U = ND->ServerConnection->URL;
			if (!U.Host.IsEmpty())
			{
				LastServerURL = FString::Printf(TEXT("%s:%d"), *U.Host, U.Port);
				bHasBeenConnected = true;
			}
		}
	}
}

FString UConnectionLifecycleSubsystem::BuildReconnectURL() const
{
	FString URL = LastServerURL;
	// Carry the persistent identity so the server can match us back to our existing rover slot.
	if (const UArtemisGameInstance* GI = Cast<UArtemisGameInstance>(GetGameInstance()))
	{
		const FString UPID = GI->GetUPID();
		if (!UPID.IsEmpty())
		{
			URL += FString::Printf(TEXT("?UPID=%s"), *UPID);
		}
	}
	return URL;
}

void UConnectionLifecycleSubsystem::BeginReconnect(const TCHAR* Reason)
{
	if (!IsClientContextNow() || !bHasBeenConnected || LastServerURL.IsEmpty())
	{
		return;
	}
	if (bReconnecting)
	{
		return; // a sequence is already running
	}
	bReconnecting = true;
	ReconnectAttempt = 0;
	UE_LOG(LogTemp, Warning, TEXT("[NetLife][CLIENT] BeginReconnect (%s) -> %s"), Reason, *LastServerURL);
	TryReconnect();
}

void UConnectionLifecycleSubsystem::ScheduleReconnect()
{
	if (!IsClientContextNow() || !bHasBeenConnected || LastServerURL.IsEmpty())
	{
		return;
	}
	if (ReconnectAttempt >= MaxReconnectAttempts)
	{
		UE_LOG(LogTemp, Error, TEXT("[NetLife][CLIENT] Giving up reconnect after %d attempts."), ReconnectAttempt);
		bReconnecting = false;
		return;
	}
	if (!GetGameInstance())
	{
		return;
	}

	const float Delay = FMath::Min(ReconnectBaseDelaySeconds * FMath::Pow(1.5f, static_cast<float>(ReconnectAttempt)), ReconnectMaxDelaySeconds);
	UE_LOG(LogTemp, Warning, TEXT("[NetLife][CLIENT] Scheduling reconnect attempt %d in %.1fs."), ReconnectAttempt + 1, Delay);

	// GameInstance timer manager: survives the level bounce to SA_Showcase (a world timer would not).
	GetGameInstance()->GetTimerManager().SetTimer(
		ReconnectTimerHandle, this, &UConnectionLifecycleSubsystem::TryReconnect, Delay, /*bLoop=*/false);
}

void UConnectionLifecycleSubsystem::TryReconnect()
{
	if (LastServerURL.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("[NetLife][CLIENT] TryReconnect: no server URL captured — cannot reconnect."));
		bReconnecting = false;
		return;
	}

	++ReconnectAttempt;
	const FString URL = BuildReconnectURL();
	UE_LOG(LogTemp, Warning, TEXT("[NetLife][CLIENT] Reconnect attempt %d -> %s"), ReconnectAttempt, *URL);

	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (APlayerController* PC = GetGameInstance() ? GetGameInstance()->GetFirstLocalPlayerController(World) : nullptr)
	{
		PC->ClientTravel(URL, TRAVEL_Absolute);
	}
	else if (GEngine && World)
	{
		// No local PC yet — drive the travel at the engine level instead.
		GEngine->SetClientTravel(World, *URL, TRAVEL_Absolute);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[NetLife][CLIENT] Reconnect: no PlayerController or World to travel with; will retry."));
		ScheduleReconnect();
	}
}

FString UConnectionLifecycleSubsystem::NetModeString() const
{
	const UWorld* W = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!W)
	{
		return TEXT("?");
	}
	switch (W->GetNetMode())
	{
	case NM_DedicatedServer: return TEXT("DEDSERVER");
	case NM_ListenServer:    return TEXT("LISTEN");
	case NM_Client:          return TEXT("CLIENT");
	case NM_Standalone:      return TEXT("STANDALONE");
	default:                 return TEXT("?");
	}
}
