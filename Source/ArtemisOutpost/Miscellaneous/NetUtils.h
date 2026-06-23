// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h" // ENetMode
#include "Kismet/BlueprintFunctionLibrary.h"
#include "NetUtils.generated.h"

/**
 * Net-role helpers for the listen-server architecture.
 *
 * The authoritative host can run either as a true dedicated server (NM_DedicatedServer,
 * NullRHI) OR as a listen server (NM_ListenServer, has an RHI so Cesium can build tile
 * collision). Both are "the server" and must skip client-only work (OculusXR anchors,
 * AR/VR pawn setup, cutout material, passthrough).
 *
 * IMPORTANT: do NOT gate client-only logic on IsLocallyControlled() alone — on a listen
 * server the host's own pawn IS locally controlled, so that check lets server-side code
 * run VR/Oculus paths on a machine with no headset. Combine it with IsClientContext().
 */
namespace ArtemisNet
{
	// Any authoritative server host — dedicated OR listen. Skip client-only work here.
	FORCEINLINE bool IsServerHost(ENetMode NetMode)
	{
		return NetMode == NM_DedicatedServer || NetMode == NM_ListenServer;
	}

	// A real client (remote Quest) or a standalone session — where VR/AR/OculusXR logic belongs.
	FORCEINLINE bool IsClientContext(ENetMode NetMode)
	{
		return NetMode == NM_Client || NetMode == NM_Standalone;
	}
}

/**
 * Blueprint-callable wrappers around the ArtemisNet helpers. Same semantics as the C++
 * functions, so BP gating matches C++ gating exactly. Resolve the net mode from the
 * world context (any actor / component / widget works as the context object).
 */
UCLASS()
class ARTEMISOUTPOST_API UArtemisNetLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// True on any authoritative server host — dedicated OR listen. Use to skip client-only work.
	UFUNCTION(BlueprintPure, Category = "Artemis|Net", meta = (WorldContext = "WorldContextObject"))
	static bool IsServerHost(const UObject* WorldContextObject);

	// True on a real client (remote) or standalone — where VR/AR/OculusXR logic belongs.
	UFUNCTION(BlueprintPure, Category = "Artemis|Net", meta = (WorldContext = "WorldContextObject"))
	static bool IsClientContext(const UObject* WorldContextObject);
};
