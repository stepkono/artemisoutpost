// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArtemisOutpost/MiniGames/General/MinigameTypes.h"
#include "HabitatTypes.generated.h"

// Sub-state of the Habitat minigame WHILE its lifecycle State is Active. Kept out of the shared
// EMinigameState on purpose: that enum travels into the registry, the aggregator's provider data and
// every puppet's ApplyState, and AMinigameActor::ServerHandleInput drops everything that is not
// exactly Active. The phase is a Habitat-only refinement of Active.
//
// The phase is DERIVED from replicated data (see HabitatRules::DerivePhase), never set by an input
// event. The server recomputes it on every ownership / lifecycle change and stores it replicated for
// Blueprint and the puppet; the View recomputes the same rule from the same replicated axes, so
// both sides agree by construction.
UENUM(BlueprintType)
enum class EHabitatPhase : uint8
{
	// State is not Active (Idle or Completed). No one is levelling.
	Inactive          UMETA(DisplayName = "Inactive"),

	// Active, but fewer than two axes are owned: at most one player is on the rotation screen.
	// Rotation is closed, dwell timers are held at zero.
	WaitingForPartner UMETA(DisplayName = "Waiting For Partner"),

	// Active and both axes are owned: both players are on the rotation screen. Rotation is open,
	// dwell accumulates, completion is evaluated.
	Leveling          UMETA(DisplayName = "Leveling")
};

// Puppet packet for the phase, pushed over the generic ApplyData channel next to the FAxisData
// stream, so the AR mirror can show "waiting" vs "levelling" without knowing the actor.
USTRUCT(BlueprintType)
struct FHabitatPhaseData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Habitat")
	EHabitatPhase Phase = EHabitatPhase::Inactive;
};

// The single definition of the phase rule. Shared by AHabitat (server, authoritative) and
// UHabitatUI (client, display) so the two can never drift apart. Header-only, no actor reference,
// so the View may include it without breaking the MVC boundary.
namespace HabitatRules
{
	// "On the rotation screen" as the server can observe it: the View shows its rotate page exactly
	// when the local player owns an axis, so "both players on the rotation screen" is "every axis has
	// an owner". Owners are always current participants because leaving releases axes.
	inline bool AreAllAxesOwned(const TArray<FAxisData>& Axes)
	{
		if (Axes.Num() == 0)
		{
			return false;
		}
		for (const FAxisData& Axis : Axes)
		{
			if (Axis.OwnerUPID.IsEmpty())
			{
				return false;
			}
		}
		return true;
	}

	inline EHabitatPhase DerivePhase(EMinigameState State, const TArray<FAxisData>& Axes)
	{
		if (State != EMinigameState::Active)
		{
			return EHabitatPhase::Inactive;
		}
		return AreAllAxesOwned(Axes) ? EHabitatPhase::Leveling : EHabitatPhase::WaitingForPartner;
	}
}
