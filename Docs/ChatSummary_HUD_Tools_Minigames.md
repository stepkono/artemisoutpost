# Chat Summary — Tools-HUD, Hand Tools & Minigame Interface

## 1. Purpose
Build the VR wrist **Tools-HUD**, the **hand tools** it launches (building placement, surface/mining scanning), and start the **mini-game interface** layer (reusable RadialWidget for rotation input, beginning with Signal Tower). Design-first, clean C++ + guided Blueprint/editor work.

## 2. Completed work
- **Tools-HUD** (`Player/PawnVR/ToolsHUD/`): client-local C++ state machine — `UToolsHUDWidget` (pages, tiles, joystick-nav, gating) + `UToolsHUDComponent` (widget host, single `OnHUDAction(EHUDAction)` event, `ConfirmSelection`). Delegate round-trip removed; widget renders into a WidgetComponent on the right controller.
- **Focus model**: menu open swaps in `IMC_HUD` (consumes stick → navigate); closed = walk. One trigger, routed in BP by state (menu confirm / active tool / world-widget).
- **Hand tools** (`Player/PawnVR/HandTools/`): `AHandToolBase` (activate/deactivate, tick only when local, registers `ACharVR::GetActiveTool()`) → `ABuildingTool`, `AScanningTool`. `ExecuteAction()`/`EndAction()` = plain virtual BlueprintCallable, routed polymorphically via `GetActiveTool()`.
- **Building placement**: `PredictArcOnSurface` — projectile arc rebuilt with per-step gravity toward the **moon surface normal** (via `GeoRefsManager`), elevation clamp, inertia (per-point lag). Server spawn via tool's own replicated RPC (client-owned Child Actor). Key gotcha: Cesium tileset **Object Type = Pawn** + Trace Complex.
- **Scanning tool**: hold-to-scan (Surface / Mining / Area), material-collection surface reveal; `DiscoverResource`/`MineResource` stubs per mode.
- **Minigame framework** (pre-existing, mapped): MVC — Actor (model) / `UMiniGameUI` (view) / `UMinigamePlayerController`. Confirmed `Axes` OnRep + `bAlwaysRelevant` broadcasts puppet data to **all** clients (OnRep ≠ Client-RPC).

## 3. Open issues / next steps
- **Next: build `WBP_RadialWidget`** — ring, fills green clockwise from 12 o'clock; `SetAngle(float 0..360)` → MID `Fill` param. User has `M_ProgressBar` + instance. Fed from `WBP_SignalTowerUI::OnAxesUpdated` → `FAxisData.Value`.
- Then: axis-selection (Claim), participant distinction, joystick→Delta rotation input (gate via `IsMiniGameActive`).
- **Watch**: `FAxisData : FMiniGameData` — USTRUCT downcast doesn't work in BP; puppet `PushData(const FMiniGameData&)` path may slice (View's `OnAxesUpdated` is fine — passes concrete `FAxisData`).
- Temporary `[BuildingTool]`/`[ScanningTool]` debug `UE_LOG`s still in — clean up later.
- Handoff prompt for the RadialWidget chat already prepared (attach `M_ProgressBar` screenshot).
