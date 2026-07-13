# ArtemisOutpost — Debugging Status

Server-authoritative rover on a Cesium moon, mirrored into AR. Two georeferences:
- **VR moon** (`CesiumGeoreference_0`, tileset `Cesium3DTileset_0`, tag `DEFAULT_TILESET`): physics; the server-authoritative `AMasterRover` drives here.
- **AR moon** (`CesiumGeoreference_1`): the visual cutout on the table; the client-side `APuppetRover` (child of the AR moon) mirrors the master.

Server drives the master via websocket → `ArtemisGameState` → `AMasterRover::HandleControls`. Clients receive the master by replication and position the puppet by converting the master's VR-moon geodetic position onto the AR moon.

---

## ✅ Fixed — Basket A: rover physics / collision / smoothing

The rover sank through / hovered over / bounced on the surface, worse the farther it drove; on the client it also drifted and "recovered" when a drive command was sent. Root-caused to **three** distinct problems:

### A1 — Client master free-simulated and sank
The client's copy of the master was running its own Chaos physics. When the server rover came to rest it slept and stopped replicating movement; during those gaps the client free-simulated under custom gravity, sank, and dragged the puppet down. A later server update snapped it back (the "send a command and it pops up" recovery).
**Fix** (`AMasterRover`): on `!HasAuthority()`, force `GetMesh()->SetSimulatePhysics(false)` every tick (the Chaos vehicle re-enables it otherwise). Position now comes purely from replication.

### A2 — Frozen client transform → apply replicated transform ourselves
With simulation off, Unreal's physics-replication path (`bRepPhysics=true`, because the *server* body simulates) stopped moving the client actor — it froze while corrections kept arriving.
**Fix** (`AMasterRover::OnRep_ReplicatedMovement`): record the replicated transform as a target and apply it on the client (via the Tick smoothing below).

### A3 — Server collision went coarse far from spawn (the "submerged" bug)
The proxy Cesium camera that should follow the rover (so Cesium streams fine collision at the rover) was **frozen at the spawn location**: the base `ACesiumCameraManager` never enables ticking and `ACustomCesiumCameraManager` had no constructor, so its `Tick()` (which repositions the proxy) never ran. Collision-tile detail was therefore anchored to spawn — measured collision `Bounds.SphereRadius` grew from ~3k near spawn to ~96k far out (a flat slab), and the rover rode that crude hull below the fine visual surface.
**Fix** (`ACustomCesiumCameraManager`): added a constructor setting `PrimaryActorTick.bCanEverTick = true`. Verified: `boundsR` stayed ~750 far from spawn; no more sinking.

### A4 — Motion smoothing
The client master snapped to each replicated transform (network cadence) → choppy.
**Fix** (`AMasterRover::Tick`): ease the **master** toward the replicated target with `VInterpTo`/`QInterpTo` (snap on first placement and on jumps > `ClientSnapDistance`); the puppet copies the already-smoothed master. Tunable on `BP_MasterRover`: `ClientLocationInterpSpeed`, `ClientRotationInterpSpeed`, `ClientSnapDistance`.

**Diagnostics used (now removed):** server `[Collision]` probe (central down-trace + 8-probe footprint fan + lateral facet scan → tile `boundsR`, coverage, `MaxSSE`); client `[BasketA] RepDiv` (local-vs-replicated divergence + correction age + `SimPhys`/`Awake`); `[CamMgr]` proxy registration/tracking. These proved: tiles were always loaded (not a streaming gap), resolution degraded with distance from spawn, and the proxy camera was registered but not following.

---

## 🔨 In progress — Basket B: long-idle disconnect → showcase level

**Confirmed cause (instrumented):** HMD doff fully *suspends* the Quest app (game thread frozen). The server drops the client after `ConnectionTimeout` (60 s observed). On re-don the app only tries to *resume the stale connection* (restart-handshake), which the server already discarded, so after another 60 s it bails to `SA_Showcase`. Two problems: (1) unavoidable disconnect on long doff; (2) no fresh reconnect.

**Design (multi-device):** the rover is server-authoritative and websocket-driven, so a sleeping Quest is only a *viewer* dropping — the rover must keep living/driving. So player slots are keyed by a **persistent cross-device UPID**; the rover is never destroyed on `Logout`; a returning Quest re-attaches to its existing slot.

**Implemented so far:**
- `UConnectionLifecycleSubsystem` client **reconnect**: captures the server URL while connected; on re-don after a long doff (or on any network failure / landing on a local map) does a fresh `ClientTravel` with `?UPID=` appended, with backoff via the GameInstance timer (survives the level bounce). `ConnectToServer(Host)` BlueprintCallable for the initial connect so it carries `?UPID` too.
- `AServerGameMode`: reads `?UPID=` in `InitNewPlayer`; keys `PlayersInGame` by UPID; **first join → `PlayerJoinDelegate`** (BP spawns, then calls `RegisterPlayerPawns`), **reconnect → `PlayerReconnectedDelegate`** (BP re-attaches the stored pawns, no spawn). `FArtemisPlayer` now stores `MasterRover`/`VRChar`.
- Bug fixes: `ArtemisGameInstance::OnStart` inverted world check; `ServerGameMode` never assigned its `GeoRefsManager` member; `PawnController::BeginPlay` was reading UPID from the server's GameInstance (now client-only + null-guarded).

**Open (BP wiring + follow-ups):** reparent the level GameMode BP to `AServerGameMode`; call `ConnectToServer` for the initial connect; bind `PlayerReconnectedDelegate` (re-attach + re-register proxy camera, no spawn) and call `RegisterPlayerPawns` after first-join spawn; ensure `Logout` never destroys the rover; **route websocket control commands to the right rover by UPID** (today all rovers react to every command); tune `AssumeDroppedAfterBackgroundSeconds` (45) below the server `ConnectionTimeout`. Optional: suppress the `SA_Showcase` bounce entirely; keep-alive on doff.

## ⏳ Later — Basket C: world re-orients on HMD re-don

**Symptom:** taking the HMD off and putting it back on rotates the moon and all Unreal-world objects (by the HMD's orientation), while spatial anchors stay put. All clients must share one world orientation.

**Hypothesis:** OpenXR/Oculus tracking-space recenter on re-don moves the tracking origin; world-space actors rotate while anchor-space anchors don't. Fix likely re-anchors the world to the spatial anchors on a recenter event. Self-contained; unrelated to physics/networking.

---

## Minor loose ends
- **Client proxy-camera registration:** on clients `AddNewMasterRover` is called with `InMasterRover=None` (BP wiring), so the client's VR tileset isn't camera-driven. Harmless today (client mirrors replication; needs no VR-moon collision), but worth fixing if a client ever needs the VR moon streamed.
- **Puppet wheel/pose animation:** with the client master not simulating, wheel spin/suspension isn't driven. If desired, replicate throttle/steer/speed and drive the puppet anim kinematically.
- **Bandwidth / net rate:** 4 rovers replicate a compact transform; if needed, lower `NetUpdateFrequency` and lean on the interpolation.
