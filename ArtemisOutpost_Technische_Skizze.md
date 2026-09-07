# ArtemisOutpost — Technische Skizze

> **Zweck dieses Dokuments:** die konkreten **Technologien** und die **technischen Anforderungen** an das
> Gesamtsystem — inklusive Abläufe und Frameworks. Es beschreibt, *wie* das System technisch aufgebaut ist
> und aufgebaut werden soll.
> **Nicht** Teil dieses Dokuments: die **Roadmap** (Stunden/Reihenfolge/Termine — separate Datei), konkrete
> **Bugs**, und das konkrete **Logging-Schema** bzw. die finalen Coupling-Metriken (offen, wird im Rahmen des
> Papers ausgearbeitet).
> Personen: **Stepan** (Unreal-Seite, Awareness Cues) · **Jan-Henrik** (Web-Layer, Bridge, Logging, Analyse).
> Stand: 22. Juli 2026.

---

## 1. Technologie-Entscheidungen (Stack)

| Baustein | Technologie |
|---|---|
| Engine | **Unreal Engine 5.7** |
| Mondterrain | **Cesium for Unreal** — echter, georeferenzierter Mond |
| Autoritativer Server | **UE Listen-Server** (Windows-Build der App) |
| Quests (AR + VR) | **UE-native App** (Quest-Build derselben App), **Meta XR SDK for Unreal** |
| Räumliche AR-Ausrichtung | **Meta Shared Spatial Anchors** |
| PC-Clients + Touchtisch | **Web-Clients** (JavaScript + **CesiumJS**) |
| Bridge | separater **WebSocket-Server** (JavaScript), Relay **und** Logging in einem Prozess |
| Datenformat Web↔UE | **JSON über WebSocket** |
| Kalibrier-Parameter | **UE DataTables** (editierbar) |
| Raum-Tracking (optional) | **OptiTrack / Motive** — Future Extension (§13) |

**Warum Listen-Server (nicht Dedicated):** Cesium for Unreal braucht eine Rendering-Layer (DirectX). Ein Dedicated Server hat die nicht, ein Listen-Server (die Windows-App) schon. Der Mond muss serverseitig gerendert werden → **es muss ein Listen-Server sein.**

> **Architektonische Konsequenz (Constraint, kein Bug):** Der Host trägt dadurch **Autorität *und* einen gerenderten Client zugleich** — schwerer als ein reiner Dedicated Server. Aktuell ist der Host „einer der PCs"; ob das für die Studie so bleibt, ist offen. Diese Doppelrolle ist beim Zuverlässigkeits-Budget mitzudenken.

---

## 2. System-Topologie

```
Quests (UE-nativ, AR+VR)  <——>  UE-Listen-Server            (Autorität für Spielzustand + Cesium-Render)
                                    |   ^
                     State-Broadcast|   | Input-Events
                                    v   |
PC-Web-Clients + Touchtisch  <——>  Bridge (ein Prozess, JS: Relay + Logging)         ← Jan-Henrik
                                     └── Telemetrie append-only ──►  Session-Logs
                                                                          │
                                                Offline-Analyse (Python) ─┘          ← Jan-Henrik

OptiTrack/Motive  ——(WLAN, Broadcast)——►  Quests        (parallel, unabhängig — nur Visualisierung; §13)
```

- **Genau eine Autorität für den Spielzustand:** der UE-Listen-Server (Single Source of Truth).
- **Die Bridge** ist ein dünnes, (nahezu) zustandsloses Relay zwischen UE und der Web-Welt und gleichzeitig die **Logging-Senke**.
- **Web = nur View + Input** — auch die Rover-Steuerung: Input hoch, State runter. Kein Web-Client hält autoritativen Spielzustand.
- **Zuständigkeiten:** Stepan besitzt die gesamte Unreal-Seite (Server-App, Quest-App, Spiellogik, Sync, Awareness Cues). Jan-Henrik besitzt die gesamte Web-Seite (PC-/Touchtisch-Clients, Bridge, Logging, Offline-Analyse, Operator-Client).

---

## 3. Koordinatenmodell

- **Lingua franca ist das echte, geodätische Mondkoordinatensystem.** Clients mit unterschiedlichen Weltkoordinatensystemen synchronisieren ihre Daten über geodätische Mondkoordinaten.
- Sowohl UE (Cesium for Unreal) als auch die Web-Clients (CesiumJS) arbeiten auf **demselben echten Cesium-Mond**.
- Eine **robuste Konvertierungsklasse** (Welt-↔ geodätische Koordinaten) existiert bereits, server- und clientseitig in der UE-App.
- Über geodätische Koordinaten werden synchronisiert: Rover- und Avatar-Positionen, Ressourcenadern (Mittelpunkt + Verlauf), Gebäude, Einflussradien, Buildability-Zonen, Scanbereiche, Touchtisch-Kartenausschnitt.
- Buildability-Zonen werden als autorierte Datenstruktur (geodätischer Mittelpunkt + Radius) definiert; die genaue Ablage ist Umsetzungsdetail.

---

## 4. Server-Autorität & Synchronisation

- **UE-Listen-Server = Single Source of Truth** für den gesamten Spielzustand (Positionen, entdeckte Gebiete, Ressourcen, Team-Inventar, Gebäude, Score, aktive Aufgaben, Touchtisch-Kartenausschnitt, AR-Kalibrierung, Spieler-Mapping).
- **Dualer Synchronisationspfad auf denselben autoritativen State:**
  1. **UE-native Replication** zwischen Server und Quests (AR/VR).
  2. **Bridge (WebSocket/JSON)** zwischen Server und Web-Clients (PC + Touchtisch).
- Clients senden **Eingaben/Absichten**; der Server entscheidet über Gültigkeit und aktualisiert den offiziellen Zustand. Der aktualisierte Zustand (bzw. relevante Ausschnitte) wird an alle Ansichten repliziert/verteilt.

---

## 5. Die Bridge (Relay + Logging in einem Prozess)

**Rolle:** dünnes, (nahezu) zustandsloses Relay. PC-/Touchtisch-Input rein → an UE weiter; UE-State raus → an die Web-Ansichten. Sie kapselt die gesamte Web-Komplexität, sodass UE schlank bleibt und Jan-Henrik die Web-Seite ohne Unreal-Interna besitzt (Deep-Modules-Prinzip auf Prozessebene).

**Warum separat (nicht in UE, nicht entfernt):** Ein Relay im LAN kostet vernachlässigbar CPU/Bandbreite; die Daten müssen die PCs ohnehin erreichen. Ein WebSocket-Server *im* UE-Listen-Server würde dagegen den Game-Thread mit Web-Protokoll belasten (Hitch-Risiko) und Jan-Henriks Arbeit an UE-Interna ketten. Direkte PC↔UE-Verbindungen würden dem autoritativen Spielserver die Verantwortung für die gesamte Web-Datenverarbeitung aufbürden — bewusst vermieden.

**Logging im selben Prozess, aber strikt getrennter logischer Kanal:**
- Node ist single-threaded in der *Ausführung*, aber I/O ist **non-blocking** (Datei-Writes laufen im Hintergrund-Threadpool). „Zwei Kanäle" = zwei Nachrichten-Typen, die derselbe Event-Loop routet — der Logging-Pfad schreibt **asynchron, append-only** (Write-Stream) und belegt den Thread nie länger als für ein triviales Puffern.
- **Harte Anforderung:** kein synchrones/blockierendes Schreiben auf dem Hot-Path (kein `*Sync`, keine schwere synchrone Verarbeitung pro Tick). Bei hoher Rate wird gebatcht (periodischer Flush). Diese logische Trennung erlaubt außerdem, das Logging später ohne Umbau in einen eigenen Prozess auszulagern.
- **Nebeneffekt:** Die Bridge sieht den State-Stream ohnehin (sie leitet ihn an die Web-Clients weiter) und kann einen Großteil der Telemetrie **direkt daraus mitloggen**. UE muss nur für Felder/Frequenzen, die die Web-Ansicht nicht trägt (z. B. feine VR-Char-Posen, hochfrequente Blickdaten), separat liefern. → verstärkt das Prinzip „State-Stream reichhaltig halten".

**Analyse-Pipeline:** ein **Offline-Batch-Job** (Python, z. B. UMAP/HDBSCAN), der die Session-Logs **nach** der Session verarbeitet — kein Live-Server, nicht latenzkritisch.

---

## 6. Clients

### 6.1 Quest — UE-nativ, AR + VR in einer App
- Eine UE-App, **zwei Modi**, Umschaltung per Controller-Button.
- Beim Wechsel zu VR wird der **VR-Character-Pawn** neben dem Rover auf der Mondoberfläche gespawnt und vom Controller auf dem UE-Server **possessed**; das HMD zeigt die Perspektive dieses VR-Characters.
- **AR-Modus:** holografische Kartendarstellung über dem physischen Touchtisch via **Meta Shared Spatial Anchors** (erledigt). Sichtbar sind gemeinsame Objekte (Rover, Avatare, Gebäude, Einflussbereiche, Fog of War) — Ressourcen bewusst nicht.

### 6.2 PC-Web-Clients (CesiumJS)
- Navigierbare Mondkarte auf dem echten Cesium-Mond; Live-Position des **eigenen** Rovers; Rover-Steuerung (Input über die Bridge).
- **Lenses:** Ressourcen, Buildability, Habitat-Einflussradius, Signal. Fog of War über nicht aufgedeckten Bereichen.
- Aktuell **gleicher Informationsstand für alle PC-Clients**. Als mögliche spätere Entwicklung notiert: an Player-IDs gekoppelte, abgrenzende Lenses (starke Tendenz aber zu „alle gleich").

### 6.3 Touchtisch-Web-Client
- Reduzierte Ansicht: Cesium-Mond zur **Navigation/Steuerung** und Steuerung des **AR-Overlays**. Der gewählte Kartenausschnitt geht an den Server und wird an die Quests verteilt, die ihre AR-Karte kontinuierlich damit synchronisieren.

---

## 7. Spieler-Identität & Geräte-Kopplung (neu zu bauen)

Ein **Session-/Identitäts-Register**, das eine Person eindeutig an **HMD ↔ Rover ↔ Avatar ↔ PC-Client** bindet. Es ist Voraussetzung für individuelle Rover-Steuerung, Per-Spieler-Logging und die Coupling-Attribution.

**Herkunft der Player-ID (aktueller Stand, noch offen):**
- Aktuell werden die Player-IDs **vom UE-Server über den PlayerController** vergeben: Für jeden PlayerController, der sich je verbunden hat, hält der Server ein persistentes Objekt; dieser PlayerController **überlebt die Trennung des Clients** und trägt die Player-ID. Die ID geht also vom UE-Server aus.
- **Vorteil für Reconnect / ID-Kontinuität:** Ein wiederverbindender Client kann an seinen bestehenden PlayerController re-attached werden und behält damit dieselbe ID (siehe Constraint unten).
- Die **Bridge** hält davon eine **Routing-Kopie** (welcher PC-Web-Client gehört zu welcher Player-ID), damit sie eingehenden PC-Input zum richtigen Rover leitet und die richtige Per-Spieler-Sicht zurückschickt. **UE bleibt autoritativ** für die Spielzustands-Konsequenzen (Possession des VR-Chars, Rover-Ownership).
- **Offen:** Es kann sich in der Entwicklung noch ändern, dass stattdessen die Bridge die IDs (1–4) selbst verwaltet, falls sich das als sinnvoller erweist.

**Mechanismus — Operator-Zuweisung (primär):**
1. Geräte melden Präsenz: **Quests** verbinden sich mit dem UE-Server (der ihren PlayerController + ID kennt) und werden an die Bridge gemeldet; **PC-Clients** melden sich direkt an der Bridge.
2. Ein **Operator-Web-Client** (Aufgabe von Jan-Henrik) listet alle online befindlichen Quests und PCs.
3. Der Operator klickt **eine Quest + einen PC** und koppelt sie — der PC-Web-Client wird damit an die **Player-ID der Quest** (aus deren PlayerController) gebunden.
4. Trennt sich ein Gerät, bleibt der PlayerController auf dem Server bestehen; bei Reconnect kann dieselbe ID wieder zugeordnet werden.
5. **Reconnect:** Die Quest-App wird neu gestartet, verbindet sich mit dem UE-Server (re-attach an den bestehenden PlayerController), sendet ihren **Geräte-Code**; UE reicht ihn an die Bridge weiter, das Gerät wird wieder als online markiert und ist mit erhaltener ID neu koppelbar.

> **Constraint — ID-Kontinuität:** Da die Coupling-Attribution an der ID hängt, sollte eine freigegebene ID nach einem Reconnect möglichst **wieder derselben Person** zugewiesen werden, nicht mitten in der Session an jemand anderen recycelt.

*Alternative (einfacher Fallback):* server-generierter **Pairing-Code**, den die Quest anzeigt und der am PC eingegeben wird. Die manuelle Freitext-ID wird wegen Kollisions-/Tippfehler-Risiko vermieden.

**Ist-Stand:** Aktuell werden **alle Rover von einem Client** gesteuert; es gibt **keine Trennung der Clients und kein PC↔HMD-Mapping**. Beides — inklusive des hier beschriebenen Registers — ist noch zu bauen.

---

## 8. Positions-Query / Weltabfrage

Eine zentrale Schnittstelle: Eingabe = geodätische Koordinaten, Rückgabe = **Datenpaket mit den Metadaten dieser Position**:
- entdeckt / unter Fog of War?
- bebaubar? bereits bebaut? welche Gebäude?
- Ressource vorhanden? welche?
- Teil eines Habitat-Einflussbereichs?
- für eine aktive Aufgabe relevant?
- welche Spieler:innen/Rover in der Nähe?

**Konsument:** PC-Lenses, Buildability, Ressourcenanzeige, Fog of War, Signal-Logik, Platzierungsvalidierung, Studienlogging.

**Ist-Stand:** Die Konvertierungsklasse (Welt↔geodätisch) ist vorhanden und robust. Die eigentliche Query-Schnittstelle ist **geplant, aber noch nicht vollständig ausgebaut**.

---

## 9. Eng gekoppelte 2-Personen-Tasks (Sync-Muster)

Für Habitat-Nivellierung und Funkmast-Ausrichtung:
- Das manipulierte Objekt (z. B. das Habitat-Fundament) existiert als **Actor in der Welt ohne Client-Ownership**.
- Zwei Clients senden ihre Eingaben per **RPC an den Server**; der **Server** verändert die Transformation autoritativ; die neue Transformation wird **an alle Clients zurückrepliziert**.

> **Zuverlässigkeits-Constraint:** Genau diese eng gekoppelten Tasks sind der heikle Teil des Syncs — Reihenfolge/Autorität der Eingaben, Interpolation, Latenz. Der autoritative Server-Pfad (Input-RPC → Server-Mutation → Replikation) ist bewusst so gewählt, dass es **kein geteiltes Ownership** eines physikalisch gegriffenen Objekts gibt.

---

## 10. Kalibrierbare Parameter

Über **UE DataTables**, **editierbar**: u. a. Scanradius, Ressourcendichte, Baukosten, Energieproduktion (Vollversion), Einflussradius, benötigte Personen pro Aufgabe, Zeitlimit. Ob Jan-Henrik diese in UE/Blueprint selbst pflegt, ist offen; die Editierbarkeit ist unabhängig davon Anforderung.

---

## 11. Logging & Coupling-Telemetrie (Anforderung — Schema offen)

- **Anforderung:** Das System muss genügend Zustand exponieren, dass sich Kopplung später **operationalisieren** lässt; die Bridge muss die entsprechenden Felder tragen (§5).
- **Bewusst offen:** das konkrete **Datenmodell/Schema** und die finalen Metriken. Das entwirft **Jan-Henrik** bzw. wird **im Rahmen des Papers** ausgearbeitet und ist **nicht** Teil dieser Skizze.
- **Ist-Stand:** Positionen existieren; darüber hinaus ist **nichts** davon umgesetzt. Zu operationalisierende Beispiele (nicht abschließend): **Spatial Proximity, Artifact Focus, Context Alignment**.
- **Kommunikation** kommt **extern** über Raum-Mikrofone (z. B. VAD) und ist **nicht** Teil des UE-States — sie wird **offline** mit den Session-Logs zusammengeführt.

---

## 12. Awareness-Cue-Schicht (Stepan)

- Eine **zusätzliche Schicht** über dem fertigen Szenario, die denselben reichhaltigen State-/Telemetrie-Stream konsumiert und Cues in VR/AR/PC rendert. Sie ist **vollständig Stepans Aufgabe** (nicht Jan-Henrik).
- **Architektur-Hook:** Weil der State-Stream ohnehin reichhaltig gehalten wird (§5), sind die Cues eine **additive** Schicht und erzwingen kein Aufmachen der einzelnen Tasks.
- Konkrete Cue-Anforderungen und ihre Umsetzung sind **noch in Arbeit** und hier bewusst offen. (Teil der Awareness ist zudem schon im Spieldesign angelegt — z. B. Fog of War als Feedthrough; siehe Szenario-Dokument.)

---

## 13. OptiTrack — Future Extension (Raum-Tracking)

Ein Feature, das voraussichtlich eingebaut wird (Stepan testet die Praxistauglichkeit). Zweck: die **echten physischen Positionen** von Personen/Objekten im Labor in VR einblenden (Workspace-Awareness + Kollisionsvermeidung).

**Topologie — Motive & UE-Server laufen parallel und unabhängig:**
- Jede Meta Quest verbindet sich **direkt mit Motive** (OptiTrack-Server) per WLAN.
- Motive **broadcastet** die Positionen aller Rigid Bodies ins Netzwerk; die Quests **empfangen nur** und senden nie Tracking-Daten zurück.
- Die **Visualisierung** der Tracking-Daten passiert **lokal auf jeder Quest**.
- Parallel läuft die normale Multiplayer-Verbindung Quests ↔ UE-Server (Spielzustand, Sync).
- **Motive und UE-Server müssen nicht verbunden sein**, da die Tracking-Daten ausschließlich der Visualisierung dienen und die Spiellogik sie nicht braucht.

**Registrierung OptiTrack ↔ Unreal:** Eine **Tracking-Kugel** wird an die Position eines **Spatial Anchors** gesetzt; darüber wird das Mapping von OptiTrack- auf Unreal-/Anker-Koordinaten hergestellt. (Der Anker ist die gemeinsame physische Referenz für physischen Raum ↔ VR-Shared-Space ↔ OptiTrack.)

Wird das Feature eingebaut, ändert sich die Topologie entsprechend (zusätzlicher Motive-Broadcast-Pfad, wie oben).

---

## 14. Codequalität — Deep Modules

Das **Deep-Modules-Prinzip** gilt unabhängig von der Aufgabenverteilung (es ist schlicht besseres Design). In UE-Begriffen: **C++-Subsysteme mit klaren, nach außen einfachen Schnittstellen** (bei Bedarf `BlueprintCallable` exponiert, sodass UMG/DataTables/Blueprint ohne C++ bespielbar sind). Komplexität wird gekapselt, nicht über das Projekt verteilt; klare Trennung von Spielzustand, Darstellung und Eingabe.

Logisch getrennte Bereiche: Koordinaten/Transformationen · Weltabfragen · Spielregeln · Ressourcenlogik · Gebäudelogik · Aufgabenlogik · Synchronisierung · Spatial Alignment · Logging · Rendering.

---

## 15. Technische Abhängigkeiten (kein Zeitplan)

Rein sachliche Abhängigkeiten — die zeitliche Planung steht in der separaten Roadmap:
- Der **Bridge/API-Contract** entblockt Jan-Henriks Web-Clients (View+Input gegen ein stabiles Protokoll).
- Ein **reichhaltiger State-Stream** ab Beginn entblockt Cues (§12) und Logging (§11) als additive Schichten.
- Das **Spieler-Identitäts-/Kopplungs-Register** (§7) ist Voraussetzung für individuelle Rover-Steuerung, Per-Spieler-Logging und Coupling-Attribution.
- Die **Positions-Query** (§8) ist Voraussetzung für Lenses, Buildability, Platzierungsvalidierung und Logging.

---

## 16. Ist-Stand (technischer Fortschritt)

| Baustein | Status |
|---|---|
| Cesium-Mond (echter Mond via Cesium for Unreal) | ✅ erledigt |
| UE-Multiplayer: Listen-Server + Client-Verbindung, mehrere Nutzer | ✅ erledigt |
| Touchtisch-Karte + Sync mit UE (steuert AR-Overlay) | ✅ erledigt |
| Fog of War (in AR) | ✅ erledigt |
| Meta Shared Spatial Anchors (AR-Ausrichtung) | ✅ erledigt |
| Wechsel AR ↔ VR (ein Client, zwei Modi, Button) | ✅ erledigt |
| Koordinaten-Konvertierung Welt ↔ geodätisch | ✅ vorhanden, robust |
| Rover-Fahren | ✅ vorhanden, **aber alle Rover von einem Client** |
| Bridge als WebSocket-Server (Relay) | ✅ vorhanden (Rolle/Logging noch zu schärfen) |
| VR-Character: Rig + vernetzte Replication | 🟡 begonnen, nicht abgeschlossen |
| Positions-Query-Schnittstelle | 🟡 geplant, nicht vollständig ausgebaut |
| Individuelle Rover-Steuerung + PC↔HMD-Mapping / Spieler-Register | ❌ noch zu bauen |
| Bridge als Logging-Senke (append-only Telemetrie) | ❌ noch zu bauen |
| Minispiele (Funkmast, Habitat, Ressourcen) + Score/Expansion | ❌ noch zu bauen |
| Web-PC-Clients (Lenses, Guidance, Signalansicht) | ❌ noch zu bauen |
| Logging-Schema, Coupling-Metriken, Analyse-Pipeline | ❌ offen (Paper) |
| Awareness-Cue-Schicht | ❌ offen (Stepan) |
| OptiTrack-Raum-Tracking | ❌ optionale Future Extension |

---

## 17. Offene technische Entscheidungen

- Ob der UE-Listen-Server bei der Studie auf einem der PCs oder auf dedizierter Hardware hostet.
- Kopplungs-Mechanismus final: Operator-Zuweisung (primär) vs. Pairing-Code (Fallback).
- Herkunft der Player-ID: UE-PlayerController (aktueller Stand) vs. Bridge-vergeben (offen).
- Player-ID-gebundene vs. einheitliche PC-Lenses.
- Logging-Schema & finale Coupling-Metriken (Paper).
- Konkrete Awareness-Cues und ihre Umsetzung (Stepan).
- Ob OptiTrack-Raum-Tracking eingebaut wird (Praxistest).
