# Mondspiel — Projekt-Kontext & Roadmap (Handoff-Dokument)

> **Zweck:** Vollständige Zusammenfassung der strategischen Planung für den CSCW-Prototyp „Mondspiel".
> Gedacht als Kontext-Startdatei für neue Chats. Stand: **20. Juli 2026**.

---

## 1. Überblick & Deadlines

Es gibt **zwei getrennte Deliverables** mit unterschiedlichen Deadlines — das ist der wichtigste strategische Punkt der ganzen Planung:

| Deliverable | Deadline | Was es *wirklich* braucht |
|---|---|---|
| **CSCW-Paper-Prototyp** | „15. Okt" (weich, projektintern) | System, das Coupling-Daten erzeugen *kann* + später die 10-Team-Studie |
| **Bachelorarbeit** | **1. Dez** (hart, extern) | Awareness Cues implementiert (bis 1. Nov) + **Experten**-Evaluation + geschriebene Arbeit |

**Kritische Erkenntnis:** Die Bachelorarbeit verlangt **nicht** die volle 10-Team-Coupling-Studie (~324.000 Coupling States). Sie verlangt Awareness Cues, die von einer Handvoll **Expert:innen** bewertet werden (Cognitive Walkthrough / heuristische Evaluation — Größenordnungen leichter als 10 Teams × 90 min). Die große Studie gehört zum *Paper* und hat keinen 1.-Dez-Zwang.

> **Offene Frage an den Betreuer (unbedingt klären):** Braucht die *Arbeit* die volle Studie, oder reicht die Experten-Evaluation der Cues? Wenn Letzteres → Plan ist machbar. Wenn die volle Studie verlangt wird → mit keiner Engine realistisch bis 1. Dez.

Weitere Anker:
- **Heute:** 20. Juli 2026
- **Partner kommt dazu:** 10. August 2026 (kann **kein UE**, nur Web)
- **Prototyp studienbereit:** 15. Oktober
- **Awareness Cues fertig:** 1. November
- **Thesis-Abgabe:** 1. Dezember

---

## 2. Tech-Stack & Projektstand

### Aktueller Stack (UE-Weg)
- **Echter Cesium-Mond** (soll erhalten bleiben)
- **Unreal Engine Multiplayer**
- **Web-Clients** (geplant, noch nicht umgesetzt)

### Bereits erledigt (nicht mehr einzuplanen)
- ✅ Cesium-Mond (echter Mond via Cesium for Unreal)
- ✅ Multiplayer-Verbindung + Rover-Spawn mehrerer Nutzer
- ✅ Shared Spatial Anchor
- ✅ Fog of War (gesamter Mond, in AR)
- ✅ Position-Query-Datenstruktur (das „Was ist an dieser Position?"-Orakel; `UMoonDataManager`)
- ✅ Rover-Fahren (aber ohne individuelle Ansteuerung)
- ✅ Touchtisch-Karte + Sync zwischen UE und Touchtisch

### Noch offen (Kurzliste)
- Viele Bugs (VR-Char, Verbindung, Optimierung)
- VR-Char: Rigging + saubere vernetzte Repräsentation + Replication
- Individuelle Rover-Ansteuerung
- Kein einziges Minispiel umgesetzt
- Keine Web-Clients
- Keine Logik für Ressourcen, Gebäude, Energie, Score
- **Kein Studien-Logging** (bisher gar nicht bedacht!)

---

## 3. Anforderungen (aus den drei Spec-Dokumenten)

Quellen: `BA Mondspiel Technische Skizze.md`, `BA Mondspiel Scenario.md`, `paper-concept.md`.

> **Hinweis zur Spec:** Die Docs beschreiben durchgängig WebXR + Three.js. Das ist laut Nutzer **nur beispielhaft** — die Anforderungen sind engine-agnostisch, ein UE-Spec ließe sich genauso schreiben. Das WebXR-in-der-Spec ist **kein** Argument für einen Pivot.

### 3.1 Systemarchitektur (Ziel)
- **Server = Single Source of Truth** (autoritativer Spielzustand)
- Pro Studiensession: **4 PC-Clients + 4 XR-Clients (Meta Quest, AR+VR) + 1 Touchtisch-Client + 1 Server**
- Eindeutige Zuordnung: **Person ↔ Rover ↔ Avatar ↔ PC-Client ↔ XR-Client**

### 3.2 Geräte-Rollen (Meta-Aufteilung, bewusst klar fürs Paper)
| Ebene | Rolle |
|---|---|
| **PC** | Individuelle Rover-Steuerung + Daten-/Visualisierungsebene (Lenses) |
| **AR / Touchtisch** | Gemeinsame Übersicht, Koordination, Awareness (kompletter Ist-Zustand) |
| **VR / HMD** | Handlungsebene (Scannen, Abbau, Platzieren, Ausrichten, Aktivieren) |

### 3.3 Kern-Loop des Spiels
1. Gebiet erkunden (Rover, primär PC) → 2. Umgebung scannen (VR) → 3. Ressource finden → 4. Ressourcenader kartografieren (2 Personen VR) → 5. Ressource abbauen (VR + PC-Guidance) → 6. Solarpanel bauen + zur Sonne ausrichten → 7. Habitat bauen (2 Personen VR, gekoppelt) → 8. Funkmast bauen + ausrichten (PC+AR+VR) → 9. Habitat aktivieren → 10. Einflussbereich erweitern → 11. Nächsten Standort erschließen.

### 3.4 Score / Expansion
- Nur **aktivierte** Habitate zählen zum Score.
- Jedes Habitat hat einen **Einflussradius**. Neues Habitat nur gültig, wenn sein Radius einen bestehenden überschneidet (einfacher Overlap-Check, keine Polygone).

### 3.5 Lenses (PC)
- Ressourcen-Lens · Buildability-Lens (rot/grün) · Habitat-Einflussradius-Lens · Signal-Lens.

### 3.6 Kalibrierbare Parameter
Scanradius, Ressourcendichte, Baukosten, Energieproduktion, Einflussradius, Solar-Nachjustierungsintervall, benötigte Personen pro Aufgabe, Zeitlimit.

---

## 4. Das Paper (Forschungsbeitrag)

**Titel-Idee:** *Coupling States: A Quantitative Framework for Describing and Discovering Coupling Styles in Transitional Interfaces.*

**Dreischichtiges Framework:**
1. **Coupling Metrics** — n normalisierte Metriken ∈ [0,1] je Dimension von Kopplung zwischen zwei Personen.
2. **Coupling States** — n-dim Vektor pro Sekunde pro Paar (bei 4er-Team = 6 Paare).
3. **Coupling Styles** — interpretierte Cluster (UMAP/t-SNE + HDBSCAN), bottom-up-Discovery + top-down-Validierung.

**Empfohlenes Kern-Metrik-Set (5 Dimensionen):**
- **Spatial Proximity** (inverse norm. Distanz im virtuellen Workspace) — Positionsdaten
- **View Overlap** (IoU sichtbarer Bereiche) — Viewport/Frustum
- **Artifact Focus** (Anteil geteilter betrachteter/manipulierter Artefakte) — Interaktionslogs
- **Context Alignment** (co-context vs. cross-context R/AR/VR) — Kontextlogs
- **Communication Intensity** (dialogische verbale Kommunikation via VAD) — Mikrofon

**Datenvolumen-Ziel:** 10 Teams × 4 Personen × 6 Paare × 90 min × 60 s = **324.000 Coupling States**.

**→ Für den Prototyp heißt das:** Das **Logging** dieser Daten ist das eigentliche Produkt fürs Paper — nicht die Grafik. Logging muss **ab Tag 1** Teil der Architektur sein.

---

## 5. Feasibility-Einschätzung

- Der **volle Scope** aus den Docs ist **solo bis 15. Okt nicht realistisch** — weder in UE noch in WebXR. Ursache ist der Umfang (9-Client-Echtzeitsystem, ~6 Minispiele, autoritativer State, Spatial Alignment, Logging), nicht Motivation oder AI-Nutzung.
- **Rest-Aufwand UE (voller Scope, ohne Erledigtes): ~15–20 Wochen.** Verfügbar bis 15. Okt: ~12 Wochen. → **Scope-Cut nötig, aber überbrückbar.**
- AI beschleunigt Boilerplate ~1,5–2×, **nicht** die harten XR-/Sync-/Coupling-Teile und **nicht** das Integrations-/Debugging. Bei Solo-XR-Projekten verbrennt die Zeit v. a. in Integration & „9 Clients zuverlässig genug für 10 Studien-Sessions".

---

## 6. Scope-Cut-Liste (falls in UE)

Geordnet von „einfach cutten, wenig Verlust" → „schmerzhaft". Maßstab „Verlust" = für Paper (Coupling-Daten) + Thesis (Awareness-Cue-Bühne).

### Tier 1 — sofort cutten, quasi kein Verlust (~2,5–3 W)
| Cut | Ersparnis | Verlust |
|---|---|---|
| Solar-Degradation / periodische Nachjustierung | ~0,3–0,5 W | keiner (reiner Wiederhol-Content) |
| Finale Assets → Platzhalter | (eh geplant) | keiner |
| Scan-Geste → Button/Dwell | ~0,3 W | Kosmetik; R→VR-Übergang bleibt |
| Ressourcenader-Rekonstruktion → Mittelpunkt+Radius+Menge | ~0,5–1 W | fast keiner; 2-Personen-Interaktion bleibt |
| Team-Inventar → simple Zähler | ~0,3 W | keiner |
| VR-Char: kein Full-Body-IK → Kopf+Hände vernetzt | ~1 W | gering; Metriken brauchen Position + Kopforientierung |

### Tier 2 — kleiner, vertretbarer Verlust (~2,5–4 W)
| Cut | Ersparnis | Verlust |
|---|---|---|
| Lenses auf 2 reduzieren (Buildability + Signal) | ~0,5 W | 2 PC-Datenpunkte weniger |
| Solarpanel-Task ganz raus, Energie einfacher | ~1 W | „Solo-VR"-Coupling-Punkt (Rover deckt loose ab) |
| Abbau in die Ader-Kartografierung falten | ~0,5–1 W | ein Cross-Reality-Mikromoment |
| Dedizierter Touchtisch-Client → simple Viewport-Steuerung | ~0,5–1 W | mittel-gering |
| Expansion: Regel behalten, nur 2–3 Habitate | ~0,3 W | gering |

### Tier 3 — nur unter echtem Druck, spürbarer Verlust
| Cut | Ersparnis | Verlust |
|---|---|---|
| Eine der zwei harten Coupling-Tasks streichen | ~1,5–2 W | **groß** — Funkmast behalten (stärkstes Cross-Reality) |
| Individuelle Rover-Steuerung weglassen | ~0,5–1 W | **groß** — ohne sie kein loose coupling |

### Tier 4 — NICHT cutten (tragend)
Autoritativer Server-State · **Logging** · mind. **eine** starke Cross-Reality-Task · **Übergang PC-Rover → VR-Astronaut** (= das Transitional Interface) · Rover-Exploration + Scan-Reveal.

**Ergebnis:** Tier 1 + Tier 2 gecuttet → verbleibendes MVP ~**10–12 W** → passt ins Fenster mit Disziplin + 1–2 W Bug-Buffer.

---

## 7. Priorität & Reihenfolge (Nutzer-Vorgabe)

1. **Funkmast zuerst** — die größte Baustelle, die stärkste PC+AR+VR-Cross-Reality-Task.
2. Braucht davor den **VR-Char**. Eine saubere vernetzte VR-Char-Repräsentation ist **zugleich ein Awareness-Cue** (Doppelnutzen Paper + Thesis).
3. **Danach die Ressourcen.**

---

## 8. Roadmap 1 — IN UE WEITERBAUEN

Baut auf erledigter Basis auf (~3 W Vorsprung), Fundament-Risiko niedrig, echter Mond bleibt. Partner (ab 10.8.) deckt Web-Layer + Logging + Analyse ab.

**Paper-Spur:**
1. **20. Jul – 3. Aug (solo):** VR-Char saubere vernetzte Repräsentation (Kopf+Hände, Replication) · Logging-Datenmodell festlegen · individuelle Rover-Steuerung.
2. **3. Aug – 10. Aug (solo):** Funkmast VR-Seite (greifen, rotieren, Ausrichtungs-Mechanik).
3. **10. Aug – 24. Aug (+Partner):** Funkmast komplett — VR-Manipulation + PC-Signalansicht + AR-Übersicht + Erfolgsbedingungen + Habitat-Aktivierung + Score. *Partner: PC-Signalansicht (Web) + Logging-Backend.*
4. **24. Aug – 14. Sep (+Partner):** Ressourcen-System — Adern (vereinfacht) + 2-Personen-VR-Kartografierung + Abbau + Team-Inventar + PC-Guidance. *Partner: PC-Guidance-Client + Analyse-Pipeline beginnen.*
5. **14. Sep – 28. Sep (+Partner):** Habitat (2-Personen-Platzierung) + Buildability + Einflussradius/Overlap/Score/Expansion + Task-State-Machine + kontextabhängige UI. *Partner: Lenses (Web) + Kalibrier-Parameter als DataTables (Blueprint).*
6. **28. Sep – 15. Okt (+Partner):** Integration, Sync-Härtung, End-to-End-Logging verifizieren, Pilot-Session, Bug-Buffer. *Partner: Analyse-Pipeline fertig + Dashboards.*
- ★ **15. Okt — Paper-Prototyp studienbereit.**

**Thesis-Spur:**
7. **15. Okt – 1. Nov (+Partner):** Awareness Cues aus VR-Char-Rep + Gaze/Proximity/Artifact-Focus (aus Logging-Metriken). *Partner: Cue-Visualisierung Web/AR.*
8. **1. Nov – 14. Nov:** Experten-Evaluation (4–6 Expert:innen). **Termine JETZT anfragen.**
9. **10. Nov – 1. Dez:** Schreiben (überlappt Eval).
- ★ **1. Dez — Bachelorarbeit-Abgabe.**

> Die volle 10-Team-Studie passt **nicht** in den Nov-Endspurt → läuft nach dem 1. Dez (davor max. reduzierter Pilot). Thesis hat Vorrang.

---

## 9. Roadmap 2 — WEBXR VON VORNE

Wirft erledigte UE-Basis weg, gewinnt vollen 2-Personen-Parallelismus ab 10.8. + schnellere Cue-Iteration. Erkauft mit riskantem Fundament.

1. **20. Jul – 10. Aug (solo) — HOCHRISIKO:** Terrain-Entscheidung (CesiumJS-GeoReference portieren vs. flaches Terrain, beides braucht Recherche) · Node-Server + Sync-Kern · WebXR-Session-Basis (AR-Passthrough + VR) · **Multi-Quest-Shared-Frame-Spike**.
   - ⚑ **10. Aug — Go/No-Go-Gate:** läuft Shared-Frame + Terrain? Sonst zurück zu UE.
2. **10. Aug – 1. Sep (2 Devs):** Fundamente neu bauen — Fog of War (GLSL) + Scan/Reveal + Position-Query + Rover + individuelle Steuerung + Player-Mapping + VR-Char-Rep.
3. **1. Sep – 21. Sep (2 Devs):** Funkmast + Ressourcen + Logging (von Anfang eingebaut).
4. **21. Sep – 15. Okt (2 Devs):** Habitat + Einflussradius/Score + Lenses + Task-State-Machine + Integration + Pilot + Bug-Buffer.
   - ★ **15. Okt — Paper-Prototyp** (*nur falls Spike gelang*).
5. **15. Okt – 1. Nov:** Awareness Cues — schnelle Iteration (Web-Vorteil).
6. **1. Nov – 1. Dez:** Experten-Evaluation + Schreiben.
   - ★ **1. Dez — Abgabe.**

---

## 10. Vergleich UE vs. WebXR

| Kriterium | UE weiterbauen | WebXR neu |
|---|---|---|
| Vorsprung | **+3 Wochen** (erledigte Basis) | 0 — Basis wird verworfen |
| Fundament-Risiko | **niedrig** | hoch (GeoReference neu, Quest-WebXR-Anchors unreif) |
| Partner-Hebel (ab 10.8.) | mittel — nur Web + Blueprint-Randaufgaben | **hoch** — voll auf gleichem Stack |
| Echter Mond / Cesium | **erhalten** | neu portieren oder flaches Terrain |
| Thesis-Cue-Iteration | langsamer | **schnell** |
| Gesamtrisiko bis 1. Dez | **mittel–niedrig** | hoch, hohe Varianz |

### Empfehlung: **In UE bleiben** (mit einer Bedingung)
Bei Funkmast-first-Plan, vorhandener UE-Basis und nur ~3 Solo-Wochen zum WebXR-De-Risking ist UE der Weg mit niedrigerer Varianz zu einer sicheren Thesis-Abgabe am 1. Dez. Das WebXR-Spec-Argument ist hinfällig (Spec ist engine-agnostisch). Die GeoReference-Neuentwicklung wäre ein teurer Frührisiko-Block.

**WebXR nur**, wenn der Go/No-Go-Spike **diese Woche** sauber gelingt **und** der Partner ~den halben Stack tragen kann. Im September zu pivoten wäre fatal.

---

## 11. Partner-Strategie (Partner kann kein UE, ab 10. Aug)

Auch ein Web-only-Partner kann in einem UE-Projekt viel übernehmen — drei saubere Schnitte:

1. **Kompletter Web-Layer:** alle PC-Clients (Rover-View, Funkmast-Signalansicht, Ressourcen-Guidance, Lenses als Overlays) — sprechen über die Bridge mit UE.
2. **Logging-Backend + Analyse-Pipeline:** Server, Speicher, Coupling-Metriken, UMAP/HDBSCAN in Python. Reine Datentechnik, kein UE nötig, = das eigentliche Paper-Produkt.
3. **Blueprint-Level in UE:** Nutzer baut C++-Scaffolding mit `BlueprintCallable`-APIs → Partner macht UMG-Widgets (HUD, Cue-Overlays) + DataTables (Kalibrier-Parameter). Blueprints in Tagen erlernbar, kein C++.
4. **Assets & Tooling:** Platzhalter-Integration, Test-Szenen, Build-/Session-Skripte.

**Web-Client-Regel:** UE bleibt autoritativ, Web-Client = View + Input. Dann sind die „1 Woche mit AI" realistisch — das Risiko ist nicht die UI, sondern die Bridge/State-Sync; die klein halten.

---

## 12. Nächste konkrete Schritte

1. **Diese Woche:** Mit Betreuer klären — braucht die Arbeit die volle Studie oder reicht Experten-Eval? (Siehe §1.)
2. **Jetzt:** Experten für die Cue-Evaluation vormerken (nicht erst im November suchen).
3. **Ab sofort (Roadmap 1, Phase 1):** VR-Char saubere Repräsentation + **Logging-Datenmodell** festlegen.
4. Logging ab Tag 1 einbauen, nicht nachrüsten.

### Offener nächster Arbeitsschritt (vorgeschlagen)
**Logging-Datenmodell konkretisieren:** welche Events/Felder pro Sekunde und pro Paar, passend zu den 5 Coupling-Metriken (Spatial Proximity, View Overlap, Artifact Focus, Context Alignment, Communication Intensity). Der Teil ist am billigsten früh und am teuersten spät.

---

## 13. Offene Entscheidungen / Risiken

- ❓ Braucht die Bachelorarbeit die volle 10-Team-Studie? (Betreuer-Frage — pivotal.)
- ❓ Engine-Endentscheidung: UE (empfohlen) vs. WebXR-Spike-Ergebnis.
- ⚠️ Eval + Schreiben in einem Monat (Nov) ist der gefährlichste Abschnitt — Experten-Termine früh und getrennt vom Schreiben legen.
- ⚠️ Zuverlässigkeit für 10 Sessions ist die eigentliche Hürde, nicht das Feature-Schreiben.
- ⚠️ Logging war bisher nicht eingeplant — jetzt Tier-4-Pflicht.

---

## 14. Referenz-Artefakt

Visueller Roadmap-Vergleich (beide Zeitpläne, Vergleichstabelle, Partner-Aufgaben):
`https://claude.ai/code/artifact/4c0ebea6-4e42-460a-a052-21c20e7650e1`

Quell-Dokumente: `BA Mondspiel Technische Skizze.md`, `BA Mondspiel Scenario.md`, `paper-concept.md`.
