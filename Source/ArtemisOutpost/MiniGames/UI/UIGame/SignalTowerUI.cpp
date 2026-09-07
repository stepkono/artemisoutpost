// Fill out your copyright notice in the Description page of Project Settings.

#include "SignalTowerUI.h"

#define LOCTEXT_NAMESPACE "SignalTowerUI"

USignalTowerUI::USignalTowerUI(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AxisTakenReason  = LOCTEXT("AxisTaken",  "Von einem anderen Spieler belegt");
	AxisSolvedReason = LOCTEXT("AxisSolved", "Bereits ausgerichtet");
}

// ---- Data in (from UMinigamePlayerController) ----

void USignalTowerUI::HandleAxesUpdated(const TArray<FAxisData>& InAxes)
{
	CachedAxes = InAxes;
	Refresh();
}

// ---- Recompute, then raise only what moved ----

void USignalTowerUI::Refresh()
{
	const TArray<FMinigameAxisOption> PrevOptions = AxisOptions;
	const int32 PrevHighlight = HighlightIndex;
	const ESignalTowerUIScreen PrevScreen = CurrentScreen;
	const bool bFirst = !bInitialBroadcastDone;

	RebuildOptions();
	UpdateScreen();

	// Page first, so the correct page is already visible when its contents update.
	if (bFirst || CurrentScreen != PrevScreen)
	{
		OnScreenChanged(CurrentScreen);
	}

	// Only when someone claimed or released an axis. A rotation tick leaves this untouched.
	if (bFirst || !OptionsEqual(PrevOptions, AxisOptions))
	{
		OnAxisOptionsChanged(AxisOptions);
	}

	// Only when the selection frame actually moved (here: auto-preselect after an ownership change).
	if (bFirst || HighlightIndex != PrevHighlight)
	{
		OnHighlightChanged(HighlightIndex, bFirst ? INDEX_NONE : PrevHighlight);
	}

	BroadcastRotation();

	bInitialBroadcastDone = true;
}

void USignalTowerUI::BroadcastRotation()
{
	int32 AxisIndex = INDEX_NONE;
	float ValueDeg = 0.0f;
	float DwellProgress = 0.0f;

	// bOwnedByLocal was already resolved in RebuildOptions, so the UPID compare happens once.
	for (const FMinigameAxisOption& Option : AxisOptions)
	{
		if (!Option.bOwnedByLocal || !CachedAxes.IsValidIndex(Option.AxisIndex))
		{
			continue;
		}

		const FAxisData& Axis = CachedAxes[Option.AxisIndex];
		AxisIndex = Option.AxisIndex;
		ValueDeg = Axis.Value;
		DwellProgress = DwellSeconds > 0.0f
			? FMath::Clamp(Axis.InToleranceTime / DwellSeconds, 0.0f, 1.0f)
			: 0.0f;
		break;
	}

	// Nothing owned (selection screen) -> stay silent instead of firing zeroes at the ring.
	if (AxisIndex == INDEX_NONE)
	{
		LastRotationAxis = INDEX_NONE;
		return;
	}

	if (AxisIndex == LastRotationAxis
		&& ValueDeg == LastRotationValue
		&& DwellProgress == LastRotationDwell)
	{
		return;
	}

	LastRotationAxis = AxisIndex;
	LastRotationValue = ValueDeg;
	LastRotationDwell = DwellProgress;

	OnRotationUpdated(AxisIndex, ValueDeg, DwellProgress);
}

// ---- Option list ----

void USignalTowerUI::RebuildOptions()
{
	AxisOptions.Reset();

	// Array position IS the axis index throughout the model (ACoupledAxisMinigameActor indexes Axes
	// directly), so the loop index is what a ClaimAxis intent must carry.
	for (int32 i = 0; i < CachedAxes.Num(); ++i)
	{
		const FAxisData& Axis = CachedAxes[i];

		FMinigameAxisOption Option;
		Option.AxisIndex = i;

		if (AxisLabels.IsValidIndex(i) && !AxisLabels[i].IsEmpty())
		{
			Option.Label = AxisLabels[i];
		}
		else
		{
			Option.Label = FText::Format(LOCTEXT("AxisFallback", "Achse {0}"), FText::AsNumber(i + 1));
		}

		Option.bOwnedByLocal = !LocalUPID.IsEmpty() && Axis.OwnerUPID == LocalUPID;
		Option.bSolved = Axis.bSolved;

		// Solved wins over ownership: a finished axis is never selectable again, by anyone.
		// Otherwise: free, or already mine. Held by another participant means greyed out.
		if (Option.bSolved)
		{
			Option.bEnabled = false;
			Option.DisabledReason = AxisSolvedReason;
		}
		else
		{
			Option.bEnabled = Axis.OwnerUPID.IsEmpty() || Option.bOwnedByLocal;
			Option.DisabledReason = Option.bEnabled ? FText::GetEmpty() : AxisTakenReason;
		}

		AxisOptions.Add(Option);
	}

	// Preselect: keep the current highlight while it is still selectable, otherwise fall to the first
	// enabled option. When one axis is taken by someone else exactly one option remains, so the only
	// viable choice ends up preselected without needing a separate rule for it.
	if (!AxisOptions.IsValidIndex(HighlightIndex) || !AxisOptions[HighlightIndex].bEnabled)
	{
		HighlightIndex = FindEnabledIndex(0, 1);
	}
}

bool USignalTowerUI::OptionsEqual(const TArray<FMinigameAxisOption>& A, const TArray<FMinigameAxisOption>& B)
{
	if (A.Num() != B.Num())
	{
		return false;
	}

	for (int32 i = 0; i < A.Num(); ++i)
	{
		// Labels come from a design-time array and never change at runtime, so only the mutable
		// fields decide whether the list would render differently.
		if (A[i].AxisIndex != B[i].AxisIndex
			|| A[i].bEnabled != B[i].bEnabled
			|| A[i].bOwnedByLocal != B[i].bOwnedByLocal
			|| A[i].bSolved != B[i].bSolved)
		{
			return false;
		}
	}
	return true;
}

int32 USignalTowerUI::FindEnabledIndex(int32 StartIndex, int32 Dir) const
{
	const int32 Num = AxisOptions.Num();
	if (Num == 0 || Dir == 0)
	{
		return INDEX_NONE;
	}

	for (int32 Step = 0; Step < Num; ++Step)
	{
		const int32 Index = (((StartIndex + Step * Dir) % Num) + Num) % Num;
		if (AxisOptions[Index].bEnabled)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

// ---- Screen ----

void USignalTowerUI::UpdateScreen()
{
	// Owning an axis IS the transition. Deriving the page from the replicated ownership (rather than
	// switching optimistically on the button press) means a claim that loses the race against another
	// participant simply leaves this player on the selection screen, with that option now greyed out.
	for (const FMinigameAxisOption& Option : AxisOptions)
	{
		if (Option.bOwnedByLocal)
		{
			CurrentScreen = ESignalTowerUIScreen::Rotate;
			return;
		}
	}

	CurrentScreen = ESignalTowerUIScreen::AxisSelection;
}

// ---- Input ----

bool USignalTowerUI::WantsNavigationInput() const
{
	return CurrentScreen == ESignalTowerUIScreen::AxisSelection;
}

void USignalTowerUI::HandleNavigate(FVector2D Axis)
{
	if (CurrentScreen != ESignalTowerUIScreen::AxisSelection || AxisOptions.Num() == 0)
	{
		return;
	}

	const float Mag = Axis.Size();

	// Stick returned near centre: re-arm so the very next push steps immediately (snappy flicks).
	if (Mag < NavReleaseThreshold)
	{
		LastNavStepTime = -1000.0f;
		return;
	}

	// Not a firm-enough push yet.
	if (Mag < NavStepThreshold)
	{
		return;
	}

	// Firm push: step on a fresh push, then auto-repeat every NavRepeatDelay while held.
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : LastNavStepTime + NavRepeatDelay;
	if ((Now - LastNavStepTime) < NavRepeatDelay)
	{
		return;
	}
	LastNavStepTime = Now;

	// Dominant-axis stepping, options read in order: right / down = next, left / up = previous.
	// (EnhancedInput thumbstick Y is +1 up, so a negative Y means "down".)
	const int32 Dir = (FMath::Abs(Axis.X) >= FMath::Abs(Axis.Y))
		? (Axis.X > 0.f ? 1 : -1)
		: (Axis.Y < 0.f ? 1 : -1);

	// Start one step away and walk on until an ENABLED option is found, so the highlight can never
	// land on a greyed-out axis.
	const int32 Start = AxisOptions.IsValidIndex(HighlightIndex) ? HighlightIndex + Dir : 0;
	const int32 Next = FindEnabledIndex(Start, Dir);

	if (Next == INDEX_NONE || Next == HighlightIndex)
	{
		return;
	}

	// Nothing else changed here, so ONLY the frame event fires. No list rebuild, no page check.
	const int32 OldIndex = HighlightIndex;
	HighlightIndex = Next;
	OnHighlightChanged(HighlightIndex, OldIndex);
}

EMiniGameUIAction USignalTowerUI::ConfirmHighlighted()
{
	// The trigger means something different per screen, and the View is the only thing that knows
	// which screen is up, so the decision belongs here. The controller only executes the result.
	if (CurrentScreen == ESignalTowerUIScreen::Rotate)
	{
		// The trigger IS the accept. There is no separate exit button and no Accept intent: the
		// player signalling "I am done" and the player leaving are the same act. Whether the tower
		// ended up solved is judged server-side once the last participant is gone, never here.
		return EMiniGameUIAction::RequestLeave;
	}

	// --- Axis selection ---

	if (!AxisOptions.IsValidIndex(HighlightIndex))
	{
		// No selectable axis (empty list, or both taken). Swallow the press so it cannot fall
		// through to the hand tool while the minigame HUD is up.
		return EMiniGameUIAction::Handled;
	}

	const FMinigameAxisOption& Option = AxisOptions[HighlightIndex];

	OnAxisOptionConfirmed(HighlightIndex, Option.bEnabled);

	if (!Option.bEnabled)
	{
		return EMiniGameUIAction::Handled;
	}

	FMinigameInput In;
	In.Type = EMinigameInputType::ClaimAxis;
	In.AxisIndex = Option.AxisIndex;
	EmitInput(In);

	// No optimistic page switch. The switch happens in UpdateScreen once the server's ownership
	// change replicates back.
	return EMiniGameUIAction::Handled;
}

#undef LOCTEXT_NAMESPACE
