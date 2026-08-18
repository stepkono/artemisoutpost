# Context: Controller-Ray-Grafik (Niagara) — für neuen Chat

**Ziel des neuen Chats:** permanente Controller-Rays entwickeln, die aus den Controllern schießen, um mit **World-Space-Widgets** zu interagieren. Grafik über **Niagara**, Interaktion über **WidgetInteractionComponent** (Laserpointer-Stil). Im Gegensatz zum HUD (Joystick-Navigation, kein Pointer) ist der Ray **permanent** sichtbar und pointer-basiert.

---

## 1. Bestehendes Niagara-Beam-Muster (wiederverwenden)

Der vorhandene Trace-Beam nutzt **`NS_TeleportTrace`** (aus dem UE-VR-Template migriert):
- **Ribbon Renderer** + Modul **„Position from Array"**, gespeist über den User-Parameter **`User.PointArray`** (Vector-Array).
- Gefüttert im BP über den Node **`Niagara Set Vector Array`** (Parameter Name exakt **`User.PointArray`**).
- **Dicke** = Ribbon Width im Emitter. **Farbe** = Kurven-Modul; Verlauf entlang des Beams über **`RibbonUV.U`** (0→1), animierbar via `frac(U - Time*Speed)`.

**Für einen geraden Controller-Ray** reicht ein Array aus wenigen Punkten, meist `[Muzzle, HitPoint]` (2 Punkte = gerade Linie). Den HitPoint liefert die WidgetInteractionComponent (`Get Last Hit Result`).

## 2. C++-Trace → Niagara-Muster (aus dem Building-Tool)

`ABuildingTool::PredictArcOnSurface` (`Source/ArtemisOutpost/Player/HandTools/BuildingTool.*`) liefert `OutPathPositions` (TArray<FVector>) → wird an `User.PointArray` gehängt. **Für den geraden Ray brauchst du diese Arc-Mathematik NICHT** — die WidgetInteractionComponent macht den Trace selbst; du fütterst nur Start+Hit in die Niagara.

**Wichtige projektspezifische Trace-Learnings (unbedingt beachten):**
- Das **Cesium3DTileset hat `Object Type = Pawn`** (nicht WorldStatic!). Object-Type-Traces müssen **`Pawn`** enthalten. Trace-Responses: `Camera = Block`, `Visibility = Ignore`.
- **Cesium-Kollision = nur Complex:** Traces mit **`Trace Complex = true`** + **Line-Trace (Radius 0)**.
- Mond = georeferenzierter Globus: „oben"/Normale ≠ Welt-Z. Geodätische Normale via `GeoRefsManager` (`UECoordsToVRMoonCoords` +Height, zurück). Für einen geraden Widget-Ray nicht nötig.

## 3. WidgetInteraction + World-Widget (neuer Teil)

- **WidgetInteractionComponent** an den (rechten/linken) Controller hängen; `Interaction Distance`, `Trace Channel` setzen. Sie macht den Trace + Klick auf World-Widgets.
- Ziel-Widgets brauchen eine **WidgetComponent** mit passender Collision, damit der Pointer sie trifft (`Receive Hardware Input` / interaktiv).
- Der **Niagara-Ray ist rein visuell**: jeden Tick Start = Component-Location, End = `WidgetInteraction → Get Last Hit Result → Impact Point` (oder Reichweiten-Ende, wenn kein Treffer) → in `User.PointArray`.
- Trigger-Klick: `WidgetInteractionComponent → Press/Release Pointer Key (LeftMouseButton)`.

## 4. HUD-Widget-Architektur (für spätere HUD-Logik-Arbeit)

- **`UToolsHUDWidget`** (C++-Basis, `Source/ArtemisOutpost/Player/ToolsHUD/`) = State-Machine: Seiten (`EToolHUDPage`), Kacheln (`FToolTile`), Highlight-Index, **Joystick-Navigation** (kein Pointer!), Gating.
- **`EHUDAction`** = alle Kachel-Actions.
- **`UToolsHUDComponent`** (auf `ACharVR`) erstellt das Widget in eine **WidgetComponent** (Tag `ToolsHUD_Anchor`) am rechten Controller. **Ein** BlueprintAssignable-Event **`OnHUDAction(EHUDAction)`** → BP macht `Switch on EHUDAction`.
- `WBP_ToolsHUD` (reparentet auf `UToolsHUDWidget`) + `WBP_ToolTile`; Events `On Page Built` / `On Highlight Changed`.
- **Wichtiger Unterschied:** HUD = Joystick+Highlight+Trigger (kein Laser). Controller-Rays = WidgetInteraction (Laser). Zwei getrennte Systeme.

## 5. Hand-Tools (Kontext)

- `AHandToolBase` → `ABuildingTool` (+ `AScanningTool` geplant), als **Child Actor Components** unter dem rechten Controller. Hide/Show statt Spawn/Destroy. Tick nur lokal (`IsOwnerLocallyControlled`).
- `ABuildingTool` sendet `ServerPlaceBuilding` selbst (client-owned via „Set Owner" + `bReplicates`), spawnt `AMinigameActor`-Subklassen (`BuildingClasses`-Map).

## Schlüssel-Dateien
- `Source/ArtemisOutpost/Player/HandTools/BuildingTool.*`, `HandToolBase.*`
- `Source/ArtemisOutpost/Player/ToolsHUD/*`
- BP/Assets: `BP_BuildingTool`, `BP_VRChar`, `WBP_ToolsHUD`, `WBP_ToolTile`, `NS_TeleportTrace`

> Hinweis: Im Building-Tool-Code stehen noch temporäre `UE_LOG(... [BuildingTool] ...)`-Debug-Zeilen — die können aufgeräumt werden.
