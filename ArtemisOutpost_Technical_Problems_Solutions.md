# ArtemisOutpost: Technical Problems and Solutions

> Technical description of the Unreal side: architecture, the hard problems met since May 2026 and how each was solved.
> The project overview is in `ArtemisOutpost_Project_Overview.md`.
> Sources: C++ in `Source/ArtemisOutpost`, Blueprint screenshots (BP_VRChar, BP_ARPawn, BP_PawnController), `DEBUGGING_STATUS.md`, `Docs/`, project notes.
> State: 2026-11-05. Unreal Engine 5.7, Cesium for Unreal, Meta XR SDK (NativeOpenXR), Quest 3.

---

## 1. Architecture in one picture

```
             Touch table (web)      PC clients (web, CesiumJS)
                     │                       │
                     └──── Bridge (Node, WebSocket, relay + logging) ────┐
                                                                        │ JSON
  ┌─────────────────────────── UE listen server (Windows, has RHI) ─────┴─────┐
  │ AArtemisGameState: MapBaseCoordinates, PlayersRoster, anchors, fog discs   │
  │ AServerGameMode: UPID slots, reconnect, pawn spawning                      │
  │ VR Moon (Cesium georef 0, full scale, collision)                           │
  │   AMasterRover (Chaos physics), ACharVR, minigame actors                   │
  └─────────────── UE replication ─────────────────────────────────────────────┘
                     │
  Quest 3 clients (same app, AR and VR mode)
    AR: APawnAR, AR Moon (Cesium georef 1) placed on the physical table,
        APuppetRover / VRCharPuppet / minigame puppets mirror the VR Moon via geodetic coords
    VR: ACharVR on the VR Moon
```

Key ideas:

- **Two Moons in one level.** The **VR Moon** is the full-size Cesium Moon. Physics, rovers, avatars and buildings live there. The **AR Moon** is a second Cesium georeference. It is rotated and positioned so that the chosen region lies on the physical table. Only visual puppets live on it.
- **Geodetic coordinates are the lingua franca.** `AGeoRefsManager` converts world ↔ lon/lat/height for both Moons (`UECoordsToVRMoonCoords`, `ARMoonCoordsToUECoords`, ...). Everything that crosses from VR to AR, or from Unreal to the web, goes through lon/lat/height.
- **Server authority.** The listen server owns all game state. Clients send intents. Server-owned actors cannot receive client RPCs, so all client → server calls go through client-owned objects (`APawnController`, its components, or client-owned child actors).
- **AR is per client.** Each Quest places the AR Moon on its own view of the table. The server never computes AR positions. AR hit tests (pointing, gaze) run on the owning client.

Main C++ modules:

| Folder | Content |
|---|---|
| `Moon/Cesium` | `AGeoRefsManager`, `UGeoUtils`, custom Cesium camera manager (server streaming around rovers) |
| `Moon/Transformation` | `UTransformationsManager` (AR Moon placement, zoom via WorldToMeters), `UVisualScaleSyncComponent` |
| `Moon/CutoutAR` | `UMapCutoutManager` (seeding from anchors), `FCutoutTileExcluder` (tile culling) |
| `Moon/MoonScannedArea`, `Moon/MoonTexturer` | Fog of war data and rendering |
| `Moon/MoonResources` | Resource veins as splines baked to geodetic samples |
| `Moon/MoonBuildings` | `UMoonMiniGamesManager` registry (habitats, towers) |
| `Anchors` | Spatial anchor creation, sharing, anchor frame maths |
| `XR` | `UXRUtilsSubsystem` (tracking-to-world cache, WTM precompensation, base orientation reset) |
| `Physics` | Custom gravity towards the Moon centre (async physics callback + CharacterMovement gravity direction) |
| `Player` | AR pawn, VR character, rovers, pawn controller, tools HUD, hand tools, controller rays, cues |
| `MiniGames` | Minigame actor framework, coupled-axis games, UI, puppets |
| `StudyData` | Event providers → `UDataAggregator` → JSON → WebSocket |
| `Networking` | WebSocket manager, connection lifecycle and reconnect |

---

## 2. Problem area A: the real Moon on a physical table (AR)

### A1. Fixing the map to the real table
**Problem:** all four Quests must see the same Moon region at the same physical spot on the table.
**Solution:** four Meta spatial anchors mark the table corners (A bottom-left, B top-left, C top-right, D bottom-right). One "authoritative" Quest creates and shares them through a group UUID. The other Quests retrieve them. `UMapCutoutManager` seeds the AR Moon from the anchors. A material parameter collection carries the corner positions, and the Moon material discards pixels outside the table rectangle.
**Side problems solved:**
- Meta XR SDK v1.201.0 had two bugs in group sharing (wrong delegate on share complete, query results skipped). Both were patched in `Plugins/MetaXR` and reported to Meta.
- A fresh group UUID per session avoids `XR_ERROR_LIMIT_REACHED`.
- Rover spawn markers were `APlayerStart` actors. The GameMode picked one of them for the AR pawn and shifted the tracking frame (map became head-locked). Replaced by `ATargetPoint`.

### A2. Touch table drives the AR map
**Problem:** the touch table chooses which Moon region lies on the table (pan, rotate, zoom).
**Solution:** the web touch table sends the geodetic corners of the visible region plus the terrain elevation (`FMapBaseCoordinates`). The server stores them in the GameState, they replicate to all Quests. `UTransformationsManager` on each Quest computes:
- **Rotation:** a rotation matrix from three region corners, so the region axes match the table axes.
- **Scale:** region edge length on the Moon divided by table edge length.
- **Position:** the Moon centre goes to `TableCenter - TableNormal × (MoonRadius + elevation)`, so the surface touches the table.
Values are interpolated every tick. Reconnects simply re-read the replicated value (`PollBaseCoordinates`).

### A3. Zoom cannot scale the Moon, so the world is scaled (WorldToMeters)
**Problem:** a 1 737 km Moon must fit on a table of about 1 m. Normal options fail:
- Scaling the georeference actor is not allowed. Rigged pawns and other actors live on the surface and cannot be scaled with it.
- Moving the Moon along the view direction changes depth. The surface leaves the table plane and stereo registration breaks.
**Solution:** zoom changes `WorldToMeters`. The Moon keeps its real size in Unreal units. The player's tracked space becomes huge instead, so 1 physical metre covers many kilometres of Moon. This was ported from the earlier `moon-rover` project.

This one decision caused most of the hard bugs below (A4 to A7).

### A4. One-frame lead of the Moon during zoom ("settle jump")
**Problem:** `SetWorldToMetersScale` is only adopted by the camera on the next tracking update. Moving the Moon in the same tick made it lead the camera by one frame. It looked smooth during zoom, then jumped when the zoom stopped.
**Solution:** an N / N+1 pipeline. Tick N computes the interpolated scale and the pending table centre and queues the WTM change. Tick N+1 commits the Moon move and the cutout corners, above any early return, so the last step is never skipped.

### A5. Precompensation around the tracking origin
**Problem:** a WTM change scales every tracked position around the XR tracking origin. The table centre and anchors therefore "move" in world space during a zoom.
**Solution:** `UXRUtilsSubsystem` caches the tracking-to-world transform at seed time. `GetXRInvariantPositionRelative` predicts where a world point lands after the scale step. `GetXRInvariantScaleAbsolute` gives authored sizes times the committed visual scale (used by late-spawned cues).

### A6. Different latency for tracked devices and anchors
**Problem:** measured on device: camera and controllers adopt a WTM change on frame N+1, spatial anchors only on N+2. Converting a live controller pose against live anchors during a zoom mixes two scales.
**Solution:** `UTrackerPoseCacheComponent` on the AR pawn caches last frame's pose (`TG_PostUpdateWork`) and the sender uses the cached pose. Anchor-derived positions (remote pawns, cues) are advanced one step in the commit event. `UVisualScaleSyncComponent` gives every AR cue a `OnVisualScaleChanged` event with exact timing and a fallback compare for late spawns.

### A7. Precision at Moon scale
**Problem:** the Moon centre is about 173 million Unreal units (cm) below the table. Sub-millimetre anchor jitter turns into jumps of hundreds of metres. Float32 maths in materials far from the origin also breaks.
**Solutions:**
- `TableZ` (table normal) is set once at seed and never updated from live anchors.
- Idle re-seeding only when anchors really moved (relocalization after HMD sleep).
- Materials never receive absolute world positions. Cue materials get per-instance offsets relative to the object bounds origin. Fog of war compares ECEF unit directions instead of positions.

### A8. AR performance: "double images"
**Problem:** in AR, virtual content appeared twice against the passthrough, worse with head speed. Measured: GameLvl AR ran at 36 to 49 of 72 FPS with many stale frames. The compositor repeated old frames. RenderDoc showed Cesium Moon tiles at about 85 % of GPU time (expensive pixel shader with clipping, fog data texture, three overlays, early-Z disabled by discard).
**Solutions so far:**
- Tileset Maximum Screen Space Error raised to 256.
- A Cesium polygon raster overlay to cull tiles outside the table: unusable, the tiles stopped rendering entirely.
- **`FCutoutTileExcluder`**: a custom `ITileExcluder` that builds a pyramid from the Moon centre through the four cutout corners (in ECEF) and excludes tiles outside it. Rendered tiles dropped from about 55 to about 10.
**Result:** 59 to 71 FPS, still not a stable 72. With the Moon hidden the app runs at 72/72 and the doubling disappears. **The Moon rendering is the remaining cost.**

---

## 3. Problem area B: standing on a sphere (VR)

### B1. Gravity
**Problem:** Unreal assumes gravity along −Z. On a globe "down" points to the Moon centre.
**Solution:** `UCustomGravityWorldSubsystem` with `UGravityAttractorComponent`. An async physics callback applies attraction to simulated bodies (rovers). Character movement components get their gravity direction updated every tick.

### B2. The horizon stayed world-up
**Problem:** with `bLockToHmd` the XR system renders the view as `TrackingToWorld × HMDPose` and ignores the camera component orientation. The player appeared to lie on the tilted ground. No camera trick could fix it.
**Solution:** rotate the whole tracking space every tick: `XRSystem->SetBaseOrientation(FindBetweenNormals(Up, ActorUp))` in `ACharVR::UpdateVRViewTilt`. On OpenXR the base orientation applies as inverse, so the inverse quaternion is fed (`bInvertVRViewTilt`).

### B3. Camera 90 cm below the feet
**Problem:** the tilt was applied twice: by the base orientation and by the tilted VROrigin parent.
**Solution:** VROrigin uses absolute rotation (world rotation zero). Its location still follows the capsule. Tracking origin `LocalFloor` for sit and stand.

### B4. Walking out of your own body
**Problem:** in room scale the HMD moves, the capsule does not.
**Solution:** `UpdateCapsuleFollowsHMD` slides the capsule under the head in the tangent plane and counter-moves VROrigin so the view does not jump.

### B5. Tilt reset and remote proxies
**Problem:** base orientation is global per machine. Remote players' VR characters must never reset it. It must also reset exactly once when leaving VR.
**Solution:** the block is gated on `bLocalVRSetupApplied` (true only for the local HMD pawn) and a reset latch per VR → AR transition.

### B6. Meta anchors carry the tilt twice
**Problem:** MetaXR applies the base orientation to anchors again by hand, so raw anchors carry the VR tilt twice.
**Solution:** `UAnchorsManagerSubsystem::TryGetAnchorsFrame` removes the tilt `artemis.AnchorTiltApplications` times (default 2), calibrates, then re-applies it once. Plugin patch postponed.

### B7. Movement and aiming on a sphere
Movement input is projected on the local tangent plane (see Blueprint section 8.3). The building tool's placement arc uses per-step gravity towards the Moon surface normal. Billboards and prompts use the actor up vector, not world up.

---

## 4. Problem area C: switching AR ↔ VR on one headset

**Flow (BP_PawnController, see screenshots):** the client sets the AR/VR context, teleports the VR character next to the rover when entering VR, then calls `RunOnServer`. The server sets the XR mode, unpossesses, and either possesses the VR character or spawns a new AR pawn and possesses it. WTM is cached and restored on each switch.

Problems solved:
- **AR Moon drifting after a VR round trip.** Root cause: in the collapsed graph `SpawnARPawn` the "Is Valid" exec pin was unwired. The VR character stayed possessed and stayed the view target in AR. Its capsule slid under gravity and dragged the tracking origin. Fixed in Blueprint. In C++ the manager now detects VR → AR, drops a pending commit, waits two ticks and resyncs scale, pivot and anchors.
- **Stale VR tilt after reconnect.** The base orientation lives on `GEngine->XRSystem` and survives a world reload. The first AR seed then saw the table upside down. Solution: the server remembers `LastARVRMode` per UPID and restores the player directly into VR (`TryRestoreIntoVR`). The AR seed waits until the local pawn is the AR pawn and the base orientation is identity for two ticks.
- **Client-side teleport of the server-owned VR character** is overwritten by the server on possession. The teleport has to be server-side or happen before possession.

---

## 5. Problem area D: Cesium on the server

- **Dedicated server crash.** A null-RHI dedicated server crashes as soon as Cesium loads tile content (texture creation on a worker thread asserts `IsInRenderingThread`). A plugin patch was tested. **Final choice: a listen server with RHI.** The Moon must exist on the server for rover physics and server traces.
- **Streaming follows cameras.** A headless server has no player camera. `ACustomCesiumCameraManager` registers a proxy camera per master rover so collision tiles stream around each rover. It must be in the level with the `DEFAULT_CAMERAMANAGER` tag.
- **Rover sank far from spawn.** The base camera manager never ticks, so the proxy camera stayed frozen at spawn. Collision tiles got coarse with distance (bounds radius from about 3k to 96k). Fix: enable ticking in the constructor.

---

## 6. Problem area E: networking and replication

| Problem | Solution |
|---|---|
| Client rover free-simulated physics during server sleep gaps and sank | Client master never simulates. `OnRep_ReplicatedMovement` sets the target, Tick eases towards it. The AR puppet copies the smoothed master. |
| VR character proxy jittered | Two Blueprint nodes wrote the capsule root every frame. Rule: exactly one writer per transform channel (location, rotation, capsule size). |
| `PlayerController` does not replicate to other clients | Cue state and tracker poses live on `AArtemisPlayerState`, replicated at 20 Hz. |
| Disconnect destroys the PlayerState, pawns survive | `AArtemisGameState::PlayersRoster` keeps every player of the session. Lookup via `FindRecordForActor`. |
| Stable identity across reconnects | Client sends `?UPID=` on connect. The server keys slots by UPID and re-attaches existing pawns. |
| HMD off suspends the app, server times out after 60 s | Reconnect with fresh `ClientTravel` and backoff. HMD on/off logged instantly via proximity delegates (app suspend events lag 15 s). |
| Physical head and hand poses of other players | Owning client sends poses relative to the **anchors frame** (table) on a timer. Server writes `FTrackerPose` to the PlayerState (`COND_SkipOwner`). Receivers rebuild them in their own anchors frame. |
| Server-owned minigame actors cannot take client RPCs | Client → server goes through `UMinigamePlayerController` on `APawnController`. Building tool uses its own RPC as a client-owned child actor. |

---

## 7. Problem area F: game systems

### F1. Fog of war
Explored area = union of scan discs, smoothed with a polynomial smooth-min. The same rule drives gameplay and rendering. Scan directions are packed into a 256×1 float data texture. A Custom node in the Cesium material compares ECEF unit directions (`dot(pixelDir, scanDir)` against an angular radius). ECEF is body-fixed, so Moon rotation cancels out.

### F2. Minigame framework
`AMinigameActor` (state, participants, input, snapshot) → `ACoupledAxisMinigameActor` (axes, claim, release, rotate, dwell) → `ASignalTower`, `AHabitat`. Rules live in components without state. Habitat phases are derived from axis ownership, not set by events. Four policy virtuals let new coupled games reuse the base. `UMoonMiniGamesManager` lets a tower claim only a completed habitat in range. AR shows visual-only puppets placed via geodetic conversion.

### F3. Tools HUD and hand tools
Client-local wrist menu (state machine in `UToolsHUDWidget`, host `UToolsHUDComponent`, one `OnHUDAction` event). Hand tools are child actors on the right controller (building, resource and area scan). Hand tools needed NoCollision, otherwise the character movement depenetrated from them and the player "flew up".
**Open (handoff 2026-11-05):** input routing derives a trigger mode every press with a fixed priority. An active tool steals the trigger from widgets and minigames. A single activity owner (`UPlayerInputModeComponent` on the pawn controller) and a generic minigame input contract are planned.

### F4. Resource veins
Splines baked into dense geodetic samples. Detection runs client-local against the client's own ray hit. The client reports only per-sample flip events. The server owns "discovered" (for queries and the bridge) and "mined out" (replicated, drives visuals).

### F5. Awareness cues
`UPlayerCuesManager` writes the player's cue state. A presenter component on each pawn (added in Blueprint) presents the cues of its own player to the local viewer, choosing AR or VR cues from the viewer's context. Cues are pure vessels written by the presenter every tick. The VR net cylinder is gated on the distance between the viewer's own trackers and the cylinder wall.

---

## 8. Blueprint walkthrough (from the screenshots)

### 8.1 BP_VRChar BeginPlay
`GatherAnimData` on every machine. Server: get GeoRefsManager → `AlignNormalWithSurface(Self, VRMoon)`. Clients: `InitConsts`, `SpawnVRCharPuppet` (the AR mirror of this avatar).

### 8.2 BP_VRChar Tick
1. `CheckCurrentContext` stops execution if the owning player is not in VR.
2. Local client: `ScaleCapsuleToHMD`. Server and other clients: `InterpTrackingSources`.
3. Every machine, body and head: mesh relative Z = −(unscaled capsule half height) − 2.15. Head yaw = rotation of the camera relative to the capsule. If |pitch| < 70°, the mesh yaw interpolates to (yaw − 90°).

### 8.3 BP_VRChar movement
`IA_Move` builds the local up vector as `normalize(ActorLocation − VRTileSet location)`. The camera forward and right vectors are projected on the tangent plane (`v − (v·up)·up`) and normalized. Two `AddMovementInput` calls follow.

### 8.4 BP_VRChar tracker replication
`ReceiveControllerChanged` (local, possessed): set `IsLocal`, `LazyInitPawnController`, `ActivateVRChar`, start the pose cache, start a looping timer at `1 / TrackerReplicationFrequency` that fires `ReplicateTransforms`. On unpossess: stop the cache and clear the timer.
`ReplicateTransforms` sends `VRTransformsToServer`: quantized world locations and rotations of camera, left and right controller, a capsule Z relative value (camera Z − capsule Z, clamped 0 to 255, rounded), the actor position, and the same three poses **relative to the anchors frame**.
Server (`VRTransformsToServerEvent`): stores the arrays, then calls `ServerSetTrackerPose` on the ArtemisPlayerState with head, left and right transforms. Other players: `InterpTrackingSources` interpolates capsule Z, tracker transforms and actor position, then aligns the normal with the surface.

### 8.5 BP_VRChar owning client
On possess: `AlignNormalWithSurface`. Binds `OnHUDAction` of the Tools HUD component and switches on `EHUDAction` (open building mode, scanning mode, build habitat, solar panel, antenna, surface scan, resource miner, area scan, close, back).

### 8.6 BP_ARPawn
BeginPlay: legacy Oculus pose offsets for the controller offsetters, `InitScales`, cache the Anchors Manager, set `IsLocalPawn` from `IsOwnedByLocalPlayer`, tracking origin `Stage`, add `IMC_ARPawn`.
`OnVisualScaleChanged` (VisualScaleSync) → `AdjustXRScales` → stores `WTMAbsoluteScalingFactor`.
Tick: lazy-init the ArtemisPlayerState via `FindRecordForActor`. Own pawn: show controller visuals by own context. Other players: hide visuals unless the **local viewer** is in AR, then `InterpTrackingSourcesAR`.

### 8.7 BP_PawnController mode switch
Client: `IA_SwitchXRMode` → switch on current mode → `SetARVRContext` (and `TeleportVRChar` when entering VR) → `RunOnServer(NewXRMode)`. Server: `SetXRMode`, log, `UnPossess`, then `Possess(VRPawn)` or `SpawnARPawn` + `Possess(ARPawn)`. Two TODOs in the graph note waiting a few ticks before calling the server and waiting for the orientation.

### 8.8 Observations worth checking (not verified)
- The capsule Z relative value uses **world Z** (`Camera.Z − Capsule.Z`). On a tilted globe position, world Z is not local up. The value can be wrong away from the spawn region.
- The movement up vector uses the location of `VRTileSet`. This is only the Moon centre if the tileset actor sits at the Moon centre. The C++ code elsewhere uses the georeference or Moon centre.
- Both points disappear if the Moon is replaced by a flat local terrain.

---

## 9. What depends on the real Moon (basis for the "fake Moon" decision)

| System | Dependency on the real Cesium Moon | Effect of a fake local terrain |
|---|---|---|
| VR terrain look | Cesium tiles from orbital data, low close-up detail | Own high-detail mesh or landscape, full art control |
| AR map on the table | Second full-size Cesium Moon, rotation + WTM zoom | Could become a scaled copy of the terrain. If the AR copy can be scaled as an actor, **WTM zoom and its whole chain (A3 to A7) disappear** |
| AR performance (A8) | Cesium tile material is about 85 % of GPU | Baked LODs and a simple material, likely 72 FPS |
| Gravity, horizon, base orientation (B1 to B6) | Needed because the surface is curved and world up ≠ local up | On a flat local terrain world up = local up. Custom gravity, view tilt, VROrigin tricks and the double anchor tilt become unnecessary |
| Geodetic conversions (`AGeoRefsManager`) | All VR ↔ AR mirroring and web sync use lon/lat/height | Replace by a local metric frame (x, y in metres on the play area) or keep a thin fake lon/lat mapping |
| Fog of war (F1) | ECEF direction compare in a Custom node | Plain 2D distance in the local frame |
| Resource veins (F4) | Baked to geodetic samples | Baked to local coordinates |
| Server Cesium streaming, proxy cameras, listen server with RHI (D) | Needed for tile collision on the server | Static collision. A dedicated server becomes possible again |
| Touch table and PC web clients | CesiumJS on the real Moon, map corners sent as lon/lat | **Biggest external impact.** Web needs the same fake terrain (custom terrain for CesiumJS, or a 2D map image). Bridge contract changes. Needs agreement with Jan-Henrik |
| Study logs | Events carry `geo:{lon,lat,height}` | Local coordinates, or fake lon/lat to keep the schema |
| Scenario | Only needs a believable lunar surface, a map and shared coordinates | Unchanged |

Constraints for the decision:
- **Time:** about 2 weeks to the study-ready prototype (20 November) and about 3.5 weeks to the thesis deadline (1 December).
- **Unreal and web must change together.** The shared coordinate system is the contract between both halves.
- A fake terrain can still be based on real lunar elevation data of one region, so the setting stays plausible.
