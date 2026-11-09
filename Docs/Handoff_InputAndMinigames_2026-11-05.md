# Handoff: VR input states, hand tools, minigame entry (2026-11-05)

Task for the new chat: fix five related issues in the VR player's input layer and then
restructure the input layer as described in "Target design" so new minigames cost no input work.
The causes below were verified against the C++ and against Blueprint screenshots on 2026-11-05,
so start from the fix plan, not from a fresh investigation.

Division of labour: the AI writes ALL C++. The user does ALL Blueprint and asset work, guided by
the AI with exact, step-by-step instructions (which Blueprint, which graph, which node to add,
delete or rewire, which property to set). Do NOT run builds, the user builds. Propose the approach
before coding (project rule). No per-tick polling for one-off events. Never add CoreRedirects for
renames: change the C++, delete the old type, and list every Blueprint node that breaks.

## Architecture as it is today

One trigger button (IA_TriggerRight, in IMC_VRChar) is routed in BP_VRChar by a Blueprint enum
`E_TriggerMode` (WorldWidgetMode, HUDMode, HandToolMode, MiniGameMode). The macro
`WhichTriggerMode` in BP_VRChar derives the mode EVERY press in this fixed priority:

1. `ToolsHUDComponent.IsMenuOpen` -> HUDMode
2. `GetActiveTool` is valid -> HandToolMode
3. `bIsInMiniGame` -> MiniGameMode
4. otherwise -> WorldWidgetMode

So WorldWidgetMode is a fallback, never a detected state: nothing checks whether the right ray
hovers a widget. Any active hand tool silently steals the trigger from world widgets AND from
minigames.

Mapping contexts are swapped only in BP_PawnController `ToggleHUDMenu(bShouldClose)`:
- open: `OpenMenu` then Add IMC_HUD (priority 10). IMC_VRChar is NOT removed.
- close: `CloseMenu` then Remove IMC_HUD then Add IMC_VRChar (priority 0).
`ToggleHUDMenu` runs from IA_ToggleHUD and from the CloseMenu case of the `OnHUDAction`
switch. It does NOT run when a Build*/Scan* tile is chosen, even though
`UToolsHUDComponent::HandleToolActionConfirmed` closes the menu in C++ for those.

State holders today:

| State | Where | Set by |
|---|---|---|
| `CurrentTriggerMode` via `WhichTriggerMode` | BP_VRChar | derived per press, see above |
| IMC_VRChar, IMC_HUD, IMC_MG_* | EnhancedInput subsystem | BP_PawnController `ToggleHUDMenu`, `ActivateMiniGameInput`, `DeactivateMiniGameInput` |
| `bToolActive` per tool + `ACharVR::ActiveHandTool` | C++ `AHandToolBase::ActivateTool/DeactivateTool` | `BeginPlacement`, `BeginScan`, BP `HolsterTool` |
| `UToolsHUDComponent::bIsOpen` | C++ | `OpenMenu/CloseMenu`, closed internally on any Build*/Scan* tile |
| `ACharVR::bIsInMiniGame` | BP read/write | BP_VRChar on `MiniGameIsOpen/Closed` |
| `APawnController::bIsPlayingMinigame` | replicated C++ | `UConnectionComponent::SetParticipantInGame` (join side commented out) |
| WidgetInteractionRight hover | component, Trace Channel WorldDynamic, Interaction Distance 750, tag Ray_Right | engine |

Key files:
- `Source/ArtemisOutpost/Player/PawnVR/ToolsHUD/ToolsHUDComponent.cpp`
- `Source/ArtemisOutpost/Player/PawnVR/HandTools/HandToolBase.cpp`, `BuildingTool.cpp`, `AResourceTool.cpp`
- `Source/ArtemisOutpost/MiniGames/UI/UIConnection/MiniGameConnectionUIComponent.cpp` (prompt, `RequestEnter` logs `[Prompt] CLICK`)
- `Source/ArtemisOutpost/MiniGames/General/GameInstance/MinigameActor.cpp` (`[Prompt] SHOWN/HIDDEN`, `RefreshLocalUI` `[View]`, reconnect self-heal in Tick)
- `Source/ArtemisOutpost/Player/PlayerController/PlayerControllerComponents/MiniGameInteraction/MinigamePlayerController.cpp`
- `Source/ArtemisOutpost/Moon/MoonBuildings/MoonMiniGamesManager.cpp` (a tower only claims a habitat whose State == Completed)
- Blueprints: BP_VRChar (`WhichTriggerMode`, trigger switches, `OnHUDAction` dispatch, `HolsterTool`), BP_PawnController (`ToggleHUDMenu`, `ActivateMiniGameInput`, IA_CloseMenu -> `RequestLeave`), BP_BuildingTool (`TogglePlacementVisual`, spawns a separate VRTeleportVisualizer actor), BP_ResourceTool, M_VRMoon Custom node reading MPC_SurfaceScannerHit, WBP_MiniGameConnectionUI (Button_0 OnClicked -> `OnConnectClicked`)
- Mapping contexts: IMC_VRChar (IA_Move, IA_Turn, IA_TriggerRight/Left, IA_ToggleHUD, pointers, both thumbsticks), IMC_HUD (IA_NavigateHUD on the right thumbstick only), IMC_MG_Habitat (IA_RotateTower on the right thumbstick, IA_CloseMenu on A)

Logging: the minigame chain logs under `LogMinigame` with tags `[UPID] [Prompt] [Enter]
[Connect] [CanJoin] [Join] [Slots] [View] [Leave] [Phase] [Rotate] [Align]`. Hand tools and HUD
log under `LogTemp` with `[BuildingTool]` and `[ToolsHUD]`. Quest: `adb logcat -d | grep LogMinigame`.

## Issue 1: pressing the connect widget opens no minigame. CONFIRMED

Cause: `WhichTriggerMode` ranks an active hand tool above everything except the HUD, and
`ABuildingTool::TryBuildAtPlacement` keeps the tool active after a build. A player who just
built a tower or habitat presses the prompt with HandToolMode, the press runs
`ExecuteAction` (another build attempt at the arc point) and `Press Pointer Key` never runs.
Signature: `[BuildingTool] TryBuildAtPlacement` at the press, no `[Prompt] CLICK`.
The same ordering makes a minigame unplayable while a tool is active: HandToolMode beats
MiniGameMode, so the minigame trigger (`SubmitEnterAction`) is also stolen.

Still true on top of that, each with a log signature:
- `[Prompt] CLICK` then `[CanJoin] REFUSED`: for a Signal Tower this is the rule that it only
  starts once a levelled (Completed) habitat is in range. Expected, but invisible to the player
  because the prompt is never hidden (`SetHiddenInGame` in `SetShowConnectionUI` is commented
  out), so a non-connectable prompt stays clickable.
- `[Connect] REFUSED ... already a participant`: reconnect left the player inside. The self-heal
  in `AMinigameActor::Tick` should reopen the View (`[View] ... reconnect recovery`).

Fix: hover must be detected, not assumed. Order the derivation as: right ray hovers an
interactable widget (`WidgetInteractionRight.IsOverInteractableWidget`) -> WorldWidget; HUD
open -> HUD; minigame View open -> MiniGame; active tool -> HandTool. Additionally holster the
active tool (and hide the placement preview) when a minigame View opens.

## Issue 2: placement preview stays after closing the building tool. CONFIRMED

`TogglePlacementVisual(false)` and `bPlacementBegan = false` run only in `TryBuildAtPlacement`.
`AHandToolBase::DeactivateTool` hides the tool actor and stops its tick but never hides the
arc, and the VRTeleportVisualizer is a separate actor spawned by BP_BuildingTool, so hiding the
tool does nothing to it. Fix: override `DeactivateTool` in `ABuildingTool`, call
`TogglePlacementVisual(false)`, reset `bPlacementBegan`, then `Super`. In BP_BuildingTool let
`TogglePlacementVisual(false)` destroy or hide the visualizer actor.

## Issue 3: too many input states. CONFIRMED

See the table. The root problem is not the number of flags, it is that four INDEPENDENT flags
(HUD open, tool active, minigame open, hover) are ranked against each other at press time, and
each flag is written from a different place. Issues 1 and 5 are both instances of that. The fix
is the "Target design" below: one owner, mutually exclusive activities, and a generic minigame
input contract. Implement it after the small patches for issues 2, 4 and 5.

## Target design

### A. One activity owner: `UPlayerInputModeComponent` on APawnController

New files: `Source/ArtemisOutpost/Player/PlayerController/PlayerControllerComponents/InputMode/PlayerInputModeComponent.h/.cpp`.
Lives on APawnController because the mapping contexts and the EnhancedInput local player
subsystem are reached from there, same as `UMinigamePlayerController`.

```
UENUM(BlueprintType)
enum class EPlayerActivity : uint8 { Free, HUDMenu, HandTool, MiniGame };

// The ONLY writer of the activity and the ONLY place that adds or removes mapping contexts.
// Tears the previous activity down before entering the new one, then swaps contexts, then logs
// "[InputMode] Old -> New (Reason)" under LogMinigame.
UFUNCTION(BlueprintCallable) void SetActivity(EPlayerActivity NewActivity, const FString& Reason);
UFUNCTION(BlueprintPure)     EPlayerActivity GetActivity() const;

// Live, derived from the right WidgetInteractionComponent (tag Ray_Right), never stored.
UFUNCTION(BlueprintPure)     bool IsHoveringWorldWidget() const;

// Single trigger entry. Replaces both "Switch on E_TriggerMode" graphs in BP_VRChar.
UFUNCTION(BlueprintCallable) void HandleTrigger(bool bPressed);
```

Teardown rules inside `SetActivity` (this is what makes states mutually exclusive):
- leaving `HUDMenu`: `UToolsHUDComponent::CloseMenu()` if still open.
- leaving `HandTool`: `AHandToolBase::DeactivateTool()` on the pawn's active tool (hides mesh,
  stops tick, and after the issue 2 fix hides the placement preview).
- leaving `MiniGame`: nothing to tear down locally, the View closes through the replicated slot
  change as today. If the activity is left for another reason while the player still holds a
  slot, call `UMinigamePlayerController::RequestLeave()`.
- entering `HandTool` while another tool is active: holster the other one first.

Context ownership (EditDefaultsOnly properties on the component, assigned in BP_PawnController):
- `LocomotionContext` = IMC_VRChar, always present, priority 0.
- `HUDContext` = IMC_HUD, present only in `HUDMenu`, priority 10.
- `MiniGameContext` = IMC_MiniGame (new, see B), present only in `MiniGame`, priority 10.
Hand tools need no context: the trigger and the ray are in IMC_VRChar.

`HandleTrigger(bPressed)` dispatch, in this order:
1. `IsHoveringWorldWidget()` -> Press/Release Pointer Key (Left Mouse Button) on the right
   WidgetInteractionComponent. Hover always wins, in every activity. This is the issue 1 fix.
2. `HUDMenu` -> on press `UToolsHUDComponent::ConfirmSelection()`.
3. `MiniGame` -> on press `UMinigamePlayerController::SubmitEnterAction()`.
4. `HandTool` -> press `ExecuteAction()`, release `EndAction()` on the active tool.
5. `Free` -> nothing.

Callers (all C++ except where noted):
- `UToolsHUDComponent::OpenMenu` -> `SetActivity(HUDMenu, "HUD opened")`.
- `UToolsHUDComponent::HandleToolActionConfirmed`: CloseMenu tile -> `Free`; Build*/Scan* tiles
  -> the BP handler calls `BeginPlacement`/`BeginScan`, and `AHandToolBase::ActivateTool` itself
  calls `SetActivity(HandTool, tool name)`. This closes issue 5 for every path.
- `AHandToolBase::DeactivateTool` -> `SetActivity(Free, "holstered")` only if this tool is the
  active one (avoid a holster-during-teardown loop: the component sets a re-entrancy guard).
- `UMinigamePlayerController::OpenUI` -> `SetActivity(MiniGame, actor name)`; `CloseUI` ->
  `SetActivity(Free, "view closed")`.
- `AMinigameActor::RefreshLocalUI` no longer calls `ActivateMiniGameInput`, the activity owner
  adds IMC_MiniGame.

What becomes dead and is DELETED (user does this in Blueprint, guided):
- BP_VRChar: enum `E_TriggerMode`, variable `CurrentTriggerMode`, macro `WhichTriggerMode`, both
  `Switch on E_TriggerMode` graphs, variable `bIsInMiniGame` (C++ property also removed). The
  IA_TriggerRight Started/Completed events each become one call to `HandleTrigger(true/false)`.
- BP_PawnController: `ToggleHUDMenu` loses its Add/Remove Mapping Context nodes (keep only the
  Open/Close calls, or delete the function and call the component directly), the whole
  `ActivateMiniGameInput` / `DeactivateMiniGameInput` section, the `OnHUDAction` switch's
  CloseMenu case (the component handles it).
- C++: `APawnController::ActivateMiniGameInput/DeactivateMiniGameInput` events, `ACharVR::bIsInMiniGame`.

### B. Generic minigame input contract

Today a new minigame costs: an IMC_MG_* asset, a case in `ActivateMiniGameInput`, an
`InputActionMap` row on the actor BP, and five `SubmitInputAction` nodes per action in
BP_PawnController. None of that is minigame-specific. Replace it with:

One mapping context `IMC_MiniGame` (user creates it, replacing IMC_MG_SignalTower, IMC_MG_Habitat,
IMC_MG_SolarPanel) with three actions the user creates:
- `IA_MG_Stick` (Axis2D, right thumbstick, dead zone modifier)
- `IA_MG_Confirm` (bool, right trigger). Optional: the activity owner's `HandleTrigger` already
  routes the trigger, so this action may be skipped. Keep ONE of the two paths, not both.
- `IA_MG_Cancel` (bool, right A) replacing IA_CloseMenu -> `RequestLeave`.

`UMinigamePlayerController` binds them once in C++ via a new
`BindInput(UEnhancedInputComponent*)` called from `ACharVR::SetupPlayerInputComponent`, the same
pattern `UToolsHUDComponent::BindInput` uses. The handlers:
- Stick Triggered: if `ActiveView->WantsNavigationInput()` -> `ActiveView->HandleNavigate(Value)`,
  else `ActiveTarget->ProcessStick(Value, Phase)`.
- Stick Completed/Canceled: `ActiveTarget->ProcessStick(ZeroVector, Phase)` so the dial gesture resets.
- Cancel Started: `RequestLeave()`.

On the model side `AMinigameActor::ProcessInput(UInputAction*, EInputActionType)` becomes
`virtual void ProcessStick(FVector2D Value, EInputActionType Phase)`. `ACoupledAxisMinigameActor`
implements it with the dial gesture it already has (move the body of the Rotate case of the
current `ProcessInput` there). `InputActionMap`, `GetLocalActionValue` and `SubmitInputAction`
are removed. Signal Tower and Habitat need no input code at all after this.

Adding a minigame then is: actor subclass, View subclass, one actor widget + one View widget,
one row in `BuildingClasses`. No input assets, no controller edits.

### C. Migration plan (C++ by the AI, Blueprint by the user)

1. C++: component, enum, `BindInput`, `ProcessStick`, callers in HUD component, hand tool base,
   minigame controller. Keep the old BP-facing functions compiling until the user has rewired,
   then delete them in a second C++ pass.
2. User, guided: create IA_MG_Stick, IA_MG_Cancel, IMC_MiniGame; assign the three contexts on
   the component in BP_PawnController; rewire IA_TriggerRight in BP_VRChar to `HandleTrigger`;
   delete the dead nodes listed in A; delete IMC_MG_* and IA_RotateTower; remove `InputActionMap`
   rows from BP_SignalTower and BP_Habitat.
3. C++ second pass: delete `ActivateMiniGameInput`, `DeactivateMiniGameInput`, `bIsInMiniGame`,
   `InputActionMap`, `SubmitInputAction`, `ProcessInput(UInputAction*)`.
4. Verify with the `[InputMode]` log: every transition prints once, no context lingers, a prompt
   press while holding a tool prints `[Prompt] CLICK`.

## Issue 4: scanner see-through dies near the body. CONFIRMED, two cutoffs

The material is NOT the cause: the Custom node in M_VRMoon is a pure 3D smoothstep around
`ScanAreaCenter` with no camera-distance term, it only depends on `ScannerFlag` and the center.
Both cutoffs are in `AResourceTool::SeeThroughSurface`:
1. The trace runs on channel `ECC_WorldStatic` and ignores only the tool actor. The VR
   character's capsule (object type Pawn) blocks that channel, so whenever the aim line crosses
   the player's own capsule the hit lands at hip height, the 3D distance from floor pixels to the
   center exceeds `ScanningRadius`, and the hole disappears. This is the cone around the body.
   Fix: `Params.AddIgnoredActor(GetOwningPawn())`, or trace by object type WorldStatic only.
2. `ScanningDistance = 200` cm from the muzzle. At shallow aims the floor is further than that,
   so the hole also dies a short way out. Raise it (600 to 1000 cm) and keep it tunable in BP.

## Issue 5: no walking after choosing a build or scan tile. CONFIRMED mechanism

`HandleToolActionConfirmed` closes the menu in C++ for every Build*/Scan* tile, but the context
swap is only in Blueprint `ToggleHUDMenu`, which runs for the CloseMenu tile and IA_ToggleHUD. So
after a tile IMC_HUD (priority 10) stays active and IA_NavigateHUD keeps consuming the right
thumbstick. Toggling the HUD open and closed finally removes it. The one detail to confirm in
IMC_VRChar is whether IA_Move or IA_Turn sits on the right thumbstick, which decides whether the
symptom is "no walking" or "no turning"; both thumbsticks are mapped in IMC_VRChar.
Fix: remove IMC_HUD on every close path. Quickest Blueprint fix: route Build*/Scan* cases of the
`OnHUDAction` switch in BP_PawnController through `ToggleHUDMenu(true)`. Proper fix: the context
add/remove moves into C++ (`OpenMenu/CloseMenu` or the input-mode owner), with the contexts as
EditDefaultsOnly properties.

## Suggested order of work

1. Issue 2 and issue 4 (small, isolated C++ changes, unblock testing).
2. Issue 5 as a Blueprint stopgap only if a build is needed before step 3: route the Build*/Scan*
   cases of the `OnHUDAction` switch in BP_PawnController through `ToggleHUDMenu(true)`. Skip it
   if step 3 follows immediately, the activity owner removes the cause.
3. Issues 1, 3 and 5 for good: implement "Target design" A, then B, following migration plan C.
   Each step ends with the exact Blueprint instructions for the user.

## Verification

1. Build, deploy both headsets and the listen server, run with `Log LogMinigame Verbose`.
2. Issue 1: build a tower, press its prompt. Before the fix: `[BuildingTool] TryBuildAtPlacement`
   and no `[Prompt] CLICK`. After: `[Prompt] CLICK`, `[Enter]`, `[Connect] GRANTED`, `[View] created`.
3. Issue 5: choose a build tile, then try the right stick. Log the active contexts from the
   input-mode owner, IMC_HUD must be gone.
4. Issue 4: print the hit actor name in `SeeThroughSurface` once, it must never be BP_VRChar.
5. Issue 2: open the building tool, holster via HUD, the arc and visualizer must be gone.
