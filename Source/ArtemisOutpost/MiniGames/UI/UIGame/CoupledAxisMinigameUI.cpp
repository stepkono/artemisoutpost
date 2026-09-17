// Fill out your copyright notice in the Description page of Project Settings.

#include "CoupledAxisMinigameUI.h"

#define LOCTEXT_NAMESPACE "CoupledAxisMinigameUI"

UCoupledAxisMinigameUI::UCoupledAxisMinigameUI(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AxisTakenReason  = LOCTEXT("AxisTaken",  "Von einem anderen Spieler belegt");
	AxisSolvedReason = LOCTEXT("AxisSolved", "Bereits ausgerichtet");
}

// ---- Data in (from UMinigamePlayerController) ----

void UCoupledAxisMinigameUI::HandleAxesUpdated(const TArray<FAxisData>& InAxes)
{
	CachedAxes = InAxes;
	Refresh();
}

void UCoupledAxisMinigameUI::HandleStateChanged(EMinigameState NewState)
{
	if (CachedState == NewState && bInitialBroadcastDone)
	{
		return;
	}
	CachedState = NewState;
	Refresh();
}

// ---- Recompute, then raise only what moved ----

void UCoupledAxisMinigameUI::Refresh()
{
	const TArray<FMinigameAxisView> PrevViews = AxisViews;
	const int32 PrevHighlight = HighlightIndex;
	const EMinigameAxisUIScreen PrevScreen = CurrentScreen;
	const bool bFirst = !bInitialBroadcastDone;

	RebuildAxisViews();
	UpdateScreen();

	// Page first, so the correct page is already visible when its contents update.
	if (bFirst || CurrentScreen != PrevScreen)
	{
		OnScreenChanged(CurrentScreen);
	}

	// Selection page: only when someone claimed or released an axis, or one got solved.
	if (bFirst || !SelectionEqual(PrevViews, AxisViews))
	{
		OnSelectionOptionsChanged(AxisViews);
	}

	// Selection frame: only when it actually moved (here: auto-preselect after an ownership change).
	if (bFirst || HighlightIndex != PrevHighlight)
	{
		OnSelectionHighlightChanged(HighlightIndex, bFirst ? INDEX_NONE : PrevHighlight);
	}

	// Rotation page: any angle, dwell, aligned flag or ownership moved. Fires with an empty array on
	// the very first refresh if the axes have not arrived yet; the BP simply has nothing to draw then.
	if (bFirst || !RotationEqual(PrevViews, AxisViews))
	{
		OnRotationAxesUpdated(AxisViews);
	}

	// Game-specific read-outs last, so the shared page state is already final when they fire.
	BroadcastGameSpecific(bFirst);

	bInitialBroadcastDone = true;
}

// ---- Per-axis view data ----

void UCoupledAxisMinigameUI::RebuildAxisViews()
{
	AxisViews.Reset();

	// Array position IS the axis index throughout the model (ACoupledAxisMinigameActor indexes Axes
	// directly), so the loop index is what a ClaimAxis intent must carry.
	for (int32 i = 0; i < CachedAxes.Num(); ++i)
	{
		const FAxisData& Axis = CachedAxes[i];

		FMinigameAxisView View;
		View.AxisIndex = i;

		if (AxisLabels.IsValidIndex(i) && !AxisLabels[i].IsEmpty())
		{
			View.Label = AxisLabels[i];
		}
		else
		{
			View.Label = FText::Format(LOCTEXT("AxisFallback", "Achse {0}"), FText::AsNumber(i + 1));
		}

		View.ValueDeg = Axis.Value;
		View.SignedDeg = FMath::UnwindDegrees(Axis.Value);
		View.DwellProgress = DwellSeconds > 0.0f
			? FMath::Clamp(Axis.InToleranceTime / DwellSeconds, 0.0f, 1.0f)
			: 0.0f;
		View.bAligned = Axis.bAligned;

		View.bOwnedByLocal = !LocalUPID.IsEmpty() && Axis.OwnerUPID == LocalUPID;
		View.bSolved = Axis.bSolved;

		// Solved wins over ownership: a finished axis is never selectable again, by anyone.
		// Otherwise: free, or already mine. Held by another participant means greyed out.
		if (View.bSolved)
		{
			View.bEnabled = false;
			View.DisabledReason = AxisSolvedReason;
		}
		else
		{
			View.bEnabled = Axis.OwnerUPID.IsEmpty() || View.bOwnedByLocal;
			View.DisabledReason = View.bEnabled ? FText::GetEmpty() : AxisTakenReason;
		}

		AxisViews.Add(View);
	}

	// Preselect: keep the current highlight while it is still selectable, otherwise fall to the first
	// enabled option. When one axis is taken by someone else exactly one option remains, so the only
	// viable choice ends up preselected without needing a separate rule for it.
	if (!AxisViews.IsValidIndex(HighlightIndex) || !AxisViews[HighlightIndex].bEnabled)
	{
		HighlightIndex = FindEnabledIndex(0, 1);
	}
}

bool UCoupledAxisMinigameUI::SelectionEqual(const TArray<FMinigameAxisView>& A, const TArray<FMinigameAxisView>& B)
{
	if (A.Num() != B.Num())
	{
		return false;
	}

	for (int32 i = 0; i < A.Num(); ++i)
	{
		// Labels come from a design-time array and never change at runtime, so only the mutable
		// fields the selection page reads decide whether it would render differently.
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

bool UCoupledAxisMinigameUI::RotationEqual(const TArray<FMinigameAxisView>& A, const TArray<FMinigameAxisView>& B)
{
	if (A.Num() != B.Num())
	{
		return false;
	}

	for (int32 i = 0; i < A.Num(); ++i)
	{
		if (A[i].ValueDeg != B[i].ValueDeg
			|| A[i].DwellProgress != B[i].DwellProgress
			|| A[i].bAligned != B[i].bAligned
			|| A[i].bOwnedByLocal != B[i].bOwnedByLocal)
		{
			return false;
		}
	}
	return true;
}

bool UCoupledAxisMinigameUI::GetOwnAxis(FMinigameAxisView& OutAxis) const
{
	for (const FMinigameAxisView& View : AxisViews)
	{
		if (View.bOwnedByLocal)
		{
			OutAxis = View;
			return true;
		}
	}
	OutAxis = FMinigameAxisView();
	return false;
}

int32 UCoupledAxisMinigameUI::FindEnabledIndex(int32 StartIndex, int32 Dir) const
{
	const int32 Num = AxisViews.Num();
	if (Num == 0 || Dir == 0)
	{
		return INDEX_NONE;
	}

	for (int32 Step = 0; Step < Num; ++Step)
	{
		const int32 Index = (((StartIndex + Step * Dir) % Num) + Num) % Num;
		if (AxisViews[Index].bEnabled)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

// ---- Screen ----

void UCoupledAxisMinigameUI::UpdateScreen()
{
	// Finished while we are still inside (Habitat): the result page wins over everything else.
	if (CachedState == EMinigameState::Completed)
	{
		CurrentScreen = EMinigameAxisUIScreen::Completed;
		return;
	}

	// Owning an axis IS the transition. Deriving the page from the replicated ownership (rather than
	// switching optimistically on the button press) means a claim that loses the race against another
	// participant simply leaves this player on the selection screen, with that option now greyed out.
	for (const FMinigameAxisView& View : AxisViews)
	{
		if (View.bOwnedByLocal)
		{
			CurrentScreen = EMinigameAxisUIScreen::Rotate;
			return;
		}
	}

	CurrentScreen = EMinigameAxisUIScreen::AxisSelection;
}

// ---- Input ----

bool UCoupledAxisMinigameUI::WantsNavigationInput() const
{
	return CurrentScreen == EMinigameAxisUIScreen::AxisSelection;
}

void UCoupledAxisMinigameUI::HandleNavigate(FVector2D Axis)
{
	if (CurrentScreen != EMinigameAxisUIScreen::AxisSelection || AxisViews.Num() == 0)
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
	const int32 Start = AxisViews.IsValidIndex(HighlightIndex) ? HighlightIndex + Dir : 0;
	const int32 Next = FindEnabledIndex(Start, Dir);

	if (Next == INDEX_NONE || Next == HighlightIndex)
	{
		return;
	}

	// Nothing else changed here, so ONLY the frame event fires. No list rebuild, no page check.
	const int32 OldIndex = HighlightIndex;
	HighlightIndex = Next;
	OnSelectionHighlightChanged(HighlightIndex, OldIndex);
}

EMiniGameUIAction UCoupledAxisMinigameUI::ConfirmHighlighted()
{
	// The trigger means something different per screen, and the View is the only thing that knows
	// which screen is up, so the decision belongs here. The controller only executes the result.
	if (CurrentScreen == EMinigameAxisUIScreen::Rotate || CurrentScreen == EMinigameAxisUIScreen::Completed)
	{
		// The trigger IS the accept. There is no separate exit button and no Accept intent: the
		// player signalling "I am done" and the player leaving are the same act. Whether the task
		// ended up solved is judged server-side, never here.
		return EMiniGameUIAction::RequestLeave;
	}

	// --- Axis selection ---

	if (!AxisViews.IsValidIndex(HighlightIndex))
	{
		// No selectable axis (empty list, or both taken). Swallow the press so it cannot fall
		// through to the hand tool while the minigame HUD is up.
		return EMiniGameUIAction::Handled;
	}

	const FMinigameAxisView& View = AxisViews[HighlightIndex];

	OnSelectionConfirmed(HighlightIndex, View.bEnabled);

	if (!View.bEnabled)
	{
		return EMiniGameUIAction::Handled;
	}

	FMinigameInput In;
	In.Type = EMinigameInputType::ClaimAxis;
	In.AxisIndex = View.AxisIndex;
	EmitInput(In);

	// No optimistic page switch. The switch happens in UpdateScreen once the server's ownership
	// change replicates back.
	return EMiniGameUIAction::Handled;
}

#undef LOCTEXT_NAMESPACE
