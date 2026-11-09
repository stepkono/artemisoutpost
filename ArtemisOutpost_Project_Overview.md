# ArtemisOutpost: Project Overview

> Abstract description of the project: what it is for, what people do in it, and how far it is.
> Technical problems and their solutions are in `ArtemisOutpost_Technical_Problems_Solutions.md`.
> Sources: the scenario, roadmap and technical sketch documents, the C++ code, the main Blueprints and the project notes.
> State: 2026-11-05.

---

## 1. What the project is

ArtemisOutpost is a **multi-device, multi-user research prototype**. Four people sit in one room and build a small lunar outpost together. Each person uses three kinds of devices: a **PC**, a **Meta Quest 3 in AR** over a physical touch table, and the **same Quest in VR** on the lunar surface.

The game is not the goal in itself. It exists to produce **collaboration data**:

1. **Paper (main purpose):** a study on *Transitional Collaboration* and *Coupling Styles*. The system records how people move between devices, views and forms of collaboration, and how tightly they work together.
2. **Bachelor thesis (Stepan):** a layer of **Awareness Cues** on top of the game. It asks how cues for workspace awareness support collaboration in transitional interfaces. Evaluation is an expert study with 4 experts.

People and responsibilities:

| Person | Area |
|---|---|
| Stepan | Everything in Unreal: server app, Quest app, game logic, sync, awareness cues |
| Jan-Henrik | Everything on the web side: PC and touch-table clients, bridge, logging, offline analysis |

Deadlines: the paper prototype should be study-ready around 20 November 2026. The thesis is due on 1 December 2026.

---

## 2. The setting

The participants are a team of four astronauts on the Moon. Each person owns exactly one **rover**, one **astronaut avatar**, one **PC seat** and one **headset**.

- At the PC, a person "sits in the rover" and drives it across the lunar landscape.
- Switching to VR means "stepping out of the rover". The avatar appears next to the person's own rover.
- In AR, everybody looks at the same holographic map of the Moon that lies on the physical table.

The team mission: explore, find resources, build habitats and activate them with signal towers. Only activated habitats count. Each activated habitat extends the area where the next one may be built. A time limit (60 to 90 minutes) ends the round. There is no target score.

---

## 3. The three device layers

Every device has one clear role. This makes the design easy to explain in the paper.

| Layer | Role | What a person sees and does |
|---|---|---|
| **PC** (web, CesiumJS) | Oversight and control | Own rover on a navigable Moon map. Drives the own rover only. Uses **lenses**: resources, buildability, habitat influence radius, signal. Unexplored areas are under fog of war. |
| **AR** (Quest passthrough over the touch table) | Coming together, coordination | A shared hologram of a Moon region on the real table. Everybody sees the same picture at the same physical spot. Shows rovers, avatars, buildings, influence areas and fog of war. Resources are deliberately hidden here. The touch table pans and zooms the region. |
| **VR** (Quest, on the surface) | Action | Walk on the Moon at human scale. Scan, map resource veins, place buildings, play the coupled minigames. |

The separation is strict: **VR acts, PC observes and steers, AR is the shared thinking space.**

Information is split on purpose. Example: resource locations are only visible on the PC. So a PC person has to guide the VR people. Collaboration does not come from roles. It comes from information asymmetry, tasks that need several people, rovers spread over the map, ordering between tasks and talking across devices.

---

## 4. Core loop and minigames

```
Explore with rover → scan area (VR) → find resource centre (PC lens)
  → map the resource vein (2 people in VR, PC guides)
  → [full version: mine resource, build and align solar panel]
  → build habitat (2 people in VR, coupled levelling)
  → build signal tower (PC + AR + VR align it)
  → habitat activated → score + larger influence area → next site
```

| Task | People | What makes it collaborative |
|---|---|---|
| **Area scan** | 1 VR | Reveals map area, buildability and resource centres for everybody. Fog of war is also a feedthrough cue: "this area is explored". |
| **Resource vein** | 2 VR + PC guidance | Only the vein centre is known. Two people walk apart along the vein. Only the PC sees where to go. |
| **Habitat** | 2 VR | Coupled two-axis levelling. Person A controls pitch, person B controls roll. A shared spirit-level bubble must stay centred for a few seconds. Nobody can fix the other axis. |
| **Signal tower** | 1 to 2 VR + PC + AR | The strongest cross-reality task. PC knows the Earth direction, AR sees the habitat direction, VR rotates the tower. The tower only activates a completed habitat in range. |
| **Solar panel** | 1 VR | Full version only. A small solo task. |

Fixed design rules:
- No fixed roles, no exclusive tools, no task claiming.
- Each task has a fixed number of VR slots.
- A new habitat must lie **inside** the influence radius of an existing activated one.
- Shared team inventory, no personal inventory.

### Plan B (cut version)

The roadmap orders work so that everything Plan B needs is built first. If time runs out, Plan B is ready automatically. Differences to the full version: no solar panel and no energy, no separate mining (mapping a vein already yields the resource), scan is a 5 second long press, habitat can fall back to a two-handle placement.

---

## 5. Awareness cue layer (thesis)

The cues sit on top of the finished game. They do not change the tasks.

Some awareness already comes from the game design itself (fog of war, visible avatars, rovers, buildings). The thesis adds:

- **Player cue state** per player: context (AR, VR or "Reality" when the HMD is off), HMD worn, activity (idle, walking, using a tool, in a minigame), talking, pointing, pointing target, gaze target.
- **Awareness HUD:** a hold-to-view overview of all players with colour, number, context and activity. Available in AR and VR.
- **Pointer rays and gaze:** where somebody points or looks, resolved to a surface position, a minigame, a player or a rover.
- **Push-to-talk signal.**
- **World cues:** the local player sees the **physical** positions of the other players' heads and hands, also while in VR. In VR this is a glowing "net cylinder" that appears when you get close to another real person (also a collision safeguard). In AR there are player tags that follow the map zoom.

---

## 6. Study data

Every relevant action becomes an **event** (context changed, activity changed, pointing, looking, talking, HMD on and off, minigame state, resource discovery). The Unreal server serializes the events as JSON and sends them to the bridge over WebSocket. The bridge writes append-only session logs. A Python pipeline (for example UMAP and HDBSCAN) analyses them offline. Room microphones record speech. Speech is merged with the logs offline.

---

## 7. Current state (2026-11-05)

| Area | State |
|---|---|
| Listen server, multiple Quests, reconnect with stable player id (UPID) | Works |
| Real Cesium Moon in VR (full scale) and as AR map on the table | Works, but AR rendering is too expensive on Quest and close-up VR terrain detail is low |
| Shared spatial anchors, AR map on the physical table, touch-table pan and zoom | Works |
| AR ↔ VR switch on the same headset, VR view aligned to the curved Moon surface | Works after many fixes |
| Rovers (server physics on the VR Moon, mirrored into AR) | Works |
| Fog of war (scan discs) | Works |
| Minigame framework, signal tower, habitat | Implemented, input routing currently being restructured |
| Tools HUD on the wrist, building and resource tools | Implemented, input states being cleaned up |
| Resource veins (discover, mine) | Implemented in C++, web forwarding open |
| Awareness cues (cue state, HUD, world cues, tracker poses) | Implemented, partly not yet device-verified |
| Event logging to the bridge | Partly (HMD, player actions, minigames, resources) |
| Web PC clients with lenses, operator client | Jan-Henrik, outside this repo |

### The open strategic question

The whole project is centred on a **real, georeferenced Cesium Moon**. Geodetic lunar coordinates are the shared language between Unreal and the web clients. Two problems come from that choice:

1. The **visual quality** of the Cesium Moon tiles is too low for a convincing close-up VR experience.
2. Showing the real Moon on a table required **scaling the world (WorldToMeters)**. This caused a long chain of hard problems (see the technical document).

The idea under consideration: **fake the Moon.** The scenario never needs the real Moon. It needs a believable lunar surface, a shared coordinate system and a map. The technical document ends with a map of everything that currently depends on the real Moon, as a basis for that decision.
