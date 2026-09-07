# ArtemisOutpost — Roadmap

## 0. Prinzip: eine Roadmap, der Cut fällt aus der Reihenfolge

Wir bauen **erst alles, was in beiden Versionen vorkommt** (primär **und** Plan B), **dann zuletzt** das, was **nur** die Primärversion hat. Geht die Zeit aus, hört man vor dem Primär-Schwanz auf — und hat damit **automatisch Plan B** studienbereit. Kein Umbau, keine Parallelplanung.

Jedes Feature ist getaggt:
- **[Kern]** — in beiden Versionen nötig (Plan B **und** primär). Wird zuerst gebaut.
- **[Primär]** — nur in der Vollversion; **Cut-Kandidat**. Wird zuletzt gebaut.
- **[BA]** — Awareness-Cue-Schicht (Stepans Bachelorarbeit). Kein Cut-Kandidat — sie ist für die Arbeit essenziell und sitzt *auf* den Kern-Mechaniken auf.

Die vier Unterschiede Plan B ↔ primär (aus dem Szenario §13) landen so von selbst am Ende: Solarpanel+Energie und der separate Ressourcen-Abbau sind **[Primär]** (echte Stunden-Ersparnis beim Cut); Scan-Geste vs. Langdruck und Habitat-Pitch/Roll vs. Zwei-Griff sind Mechanik-Varianten *innerhalb* eines Kern-Features (im Build entschieden, kaum Stundenunterschied).

---

## 1. Kernannahmen

- **Deadlines:** **Paper-Prototyp muss im Oktober studienbereit sein** (inkl. paper-grade Härtung — kein Aufschub). **1. Dezember = Bachelorarbeit-Abgabe.**
- **Thesis = Experten-Studie mit 4 Experten** (keine volle Studie). Studiendesign des Papers = Betreuer.
- **Zwei Zuverlässigkeits-Stufen, beide im Oktober:** thesis-grade (geführte 4-Experten-Sessions) + paper-grade (10 Sessions).
- **Kapazität Stepan (eigene Angabe: „beast"):** Mo–Fr ~11 h Prototyp + ~1,5 h Schreiben, Wochenende ~16 h → grob **~55 h/Woche Prototyp** + **~15–18 h/Woche Schreiben**.
- Prototyp-Stunden = Netto-Arbeit; der **Schreib-/Eval-Track ist separat** ausgewiesen (§4).

---

## 2. Die Roadmap (Bau-Reihenfolge)

| Phase | Reihe | Feature | Tag | Owner | h | Notiz |
|---|:--:|---|:--:|:--:|--:|---|
| **FIRST CONTACT** | **P0** | AR-Rotations- + VR-Spawn-Bug fixen | Kern | Stepan | 3 | blockiert nutzbare Sessions — in Prüfungsphase |
| **FIRST CONTACT** | **P0** | Bridge/API-Contract + reichhaltiger State-Stream | Kern | Stepan | 20 | muss früh stehen – "unleashed" Jan-Henrik |
| **FIRST CONTACT** | **P0b** | VR-Char Rig + Replication fertig | Kern | Stepan | 5 | fast fertig; nötig für Funkmast-VR; zugleich Cue |
| **FIRST CONTACT** | **P0c** | Ind. Rover-Steuerung + Spieler-Register + PC↔HMD-Mapping | Kern | Stepan | 20 | Player-ID via UE-PlayerController; Voraussetzung für alles Weitere |
| **THE BEST OF BOTH WORLDS** | **1** | **Funkmast** bauen + ausrichten | Kern | Stepan | 45 | Prio 1, stärkste Cross-Reality-Task |
| **PROJECT GENESIS** | **2** | **Habitat** bauen + ausrichten | Kern | Stepan | 66 | Prio 2; Pitch/Roll primär, Zwei-Griff-Fallback (= Plan-B-Mechanik); inkl. Buildability, Radius/Overlap, Score, State-Machine, Kontext-UI |
| **THE FINAL FRONTIER** | **3** | **Ressource kartografieren** (2-P-VR) | Kern | Stepan | 30 | Prio 3; **Ader voll** (2 Personen vom Mittelpunkt auseinander); erzeugt in der Kern-Logik bereits die Ressource |
| **THE FINAL FRONTIER** | — | Positions-Query ausbauen | Kern | Stepan | 12 | speist Lenses, Buildability, Platzierung, Logging |
| **THE FINAL FRONTIER** | — | Kalibrier-Parameter (DataTables) | Kern | Stepan | 8 | editierbar; ggf. Jan-Henrik, falls er Blueprints macht |
| **THE MEASURE OF A MAN** | **6** | Logging-Felder exponieren (Bridge) | Kern | Stepan | 12 | nur Felder sicherstellen; Modell+Backend+Pipeline = Jan-Henrik |
| **THE INNER LIGHT** | **5** | **Awareness-Cues** auf Tasks | BA | Stepan | 40 | Prio 5; parallele BA-Schicht auf den Kern-Tasks; **kein Cut** |
| **KOBAYASHI MARU** | — | Integration + Pilot | Kern | Stepan | 60 | laufend |
| **KOBAYASHI MARU** | — | Sync-Härtung & Optimierung | Kern | Stepan | 60 | thesis- **und** paper-grade, **komplett im Oktober** |
| **KOBAYASHI MARU** | — | Bug-Buffer (~15 %) | Kern | Stepan | ~58 | |
| **THE NEUTRAL ZONE** | ▸ | **— ab hier nur noch Primärversion —** | | | | *hört man vorher auf → Plan B ist fertig* |
| **THE UNDISCOVERED COUNTRY** | **4** | **Ressource-Abbau** (separater Schritt) | Primär | Stepan | 12 | Plan B: entfällt (Kartografieren erzeugt die Ressource) |
| **THE UNDISCOVERED COUNTRY** | — | **Solarpanel-Task + Energie-Ökonomie** | Primär | Stepan | 30 | Plan B: entfällt ganz (Gebäude kosten nur Regolith) |
| **THE UNDISCOVERED COUNTRY** | — | **Scan als VR-Geste** (statt 5-Sek-Langdruck) | Primär | Stepan | 8 | Plan B: Langdruck genügt |
| **THE UNDISCOVERED COUNTRY** | — | Bug-Buffer auf Primär-Anteil | Primär | Stepan | ~8 | |

---

## 3. Stunden-Summen (Prototyp)

**Stepan (Unreal-Seite):**

| | Stunden |
|---|--:|
| **Kern + Cues + Integration + Härtung + Buffer** = **Plan B studienbereit** | **≈ 450 h** *(Range 400–520)* |
| davon Awareness-Cues (BA-Schicht) | 40 |
| **+ Primär-Schwanz** (Abbau, Solarpanel/Energie, Scan-Geste, Buffer) | +58 |
| **= Vollversion** | **≈ 510 h** *(Range 460–590)* |

**Jan-Henrik (Web/Bridge/Logging/Analyse):**

| Paket | h |
|---|--:|
| Bridge (Relay + Logging-Senke, ein Prozess) bauen/schärfen | 30 |
| Operator-Client (Geräte-Kopplung, Online-Liste) | 12 |
| Web-PC-Clients: Rover-View, Lenses (2), Guidance, Funkmast-Signal | 95 |
| Datenmodell + Logging-Backend/Schema *(offen, Paper)* | 50 |
| Analyse-Pipeline (offline, UMAP/HDBSCAN, Python) | 50 |
| **Summe Jan-Henrik** | **≈ 237 h** |

> Die Prototyp-Summen sind gegenüber früher leicht gestiegen, weil der Technik-Durchgang echte Bausteine sichtbar gemacht hat, die vorher nicht einzeln standen (Spieler-Register/Mapping, Positions-Query). Der Ader-„Cut" ist bewusst **entfallen** (Ader ist in beiden Versionen voll).

---

## 4. Schreib- & Eval-Track (separat!)

| Paket | h |
|---|--:|
| Experten-Studie (4 Experten): Material, Termine, 4 Sessions, Auswertung | ~30 |
| Bachelorarbeit schreiben (Intro, Related Work, Konzept, System, Method, Results, Discussion) | ~180 |
| **Summe Thesis-Track (Stepan)** | **≈ 210 h** *(Range 180–290)* |

Dieser Track läuft in Abend-/Wochenend-Slots (~15–18 h/Woche) parallel zum Bau und wird ab dem 1. Dez zur Hauptlast.

---

## 5. Kalender (beide Tracks)

| Zeitraum                                 | Prototyp-Track                                                                             | Schreib-Track                                                            |
| ---------------------------------------- | ------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------ |
| **10.08.–26.08.** (Prüfungen, reduziert) | P0: Bugs + Bridge/API-Contract (~36 h)                                                     | —                                                                        |
| **26.08.–30.08.**                        | VR-Char (~5 h) + Rover-Steuerung/Spieler-Register + Funkmast-VR-Start                      | —                                                                        |
| **30.08.–20.09.** (+Jan-Henrik)          | **Funkmast** komplett [Kern]                                                               | —                                                                        |
| **21.09.–11.10.**                        | **Habitat** komplett [Kern]                                                                | Related Work + Konzept (~30 h)                                           |
| **11.10.–28.10.**                        | **Ressource kartografieren** [Kern] + Positions-Query + Logging-Felder + DataTables        | System-/Implementierungs-Kapitel (~30 h)                                 |
| **28.10.–~09.11.**                       | **Awareness-Cues** [BA] + Integration + thesis-grade Härtung + Pilot                       | Weiterschreiben (~30 h)                                                  |
| ★ **~09.11.**                            | **Kern-Prototyp + Cues bereit → Plan B studienbereit**                                     |                                                                          |
| **~09.11.–20.11.**                       | **[Primär] falls Zeit:** Abbau, Solarpanel+Energie, Scan-Geste · **+ paper-grade Härtung** | —                                                                        |
| ★ **20.11.**                             | **Paper-Prototyp studienbereit** (voll, falls Primär gebaut; sonst Plan B)                 |                                                                          |
| **21.11.–25.11.**                        | (nur noch Bugfixes on demand)                                                              | **Experten-Studie (4)** + Method + Results-Start (~40 h)                 |
| **25.11.–01.12.**                        | —                                                                                          | **Schreiben "beast-mode"-intensiv:** Results, Discussion, Conclusion, Politur (~80 h) |
| ★ **~01.12.**                            |                                                                                            | **Bachelorarbeit-Abgabe**                                                |

**So wirkt der Cut:** Der Primär-Schwanz sitzt bewusst in den letzten Novembertagen. Liegst du zurück, lässt du ihn weg — Plan B ist ab ~09.11. fertig, und die freien Novembertage gehen in Härtung + Eval-Vorbereitung.

**Last-Check (beast):** Plan-B-Prototyp ~450 h bei ~55 h/Woche ≈ **8 Wochen** reine Bauzeit; die Vollversion ~510 h ≈ **9 Wochen**. Ins Fenster 26.08.→20.11. (~12 Wochen) passt beides, die Vollversion mit weniger Puffer.


---

## 6. Jan-Henrik, ab ~10.08

~237 h über ~9 Wochen ≈ **~26 h/Woche** → gut ausgelastet. Drei saubere Schnitte, alle **ohne Unreal**: der komplette Web-Layer (PC-Clients als View+Input über die Bridge), Bridge + Logging-Senke + Analyse-Pipeline (das eigentliche Paper-Produkt), sowie der Operator-Client für die Geräte-Kopplung.

**Die blockierende Abhängigkeit:** Jan-Henrik baut gegen den **Bridge/API-Contract**. Das **Datenmodell** entwerft ihr in seiner ersten Woche gemeinsam; die Bridge muss die dafür nötigen Felder tragen.

---

## 7. Risiken

- ⚠️ **Sync-Zuverlässigkeit** ist der größte Varianztreiber — und da der Paper-Prototyp im Oktober studienbereit sein muss, fällt die **paper-grade Härtung komplett in den Oktober**. Engster Punkt des Bauplans.
- ⚠️ **Bridge-Contract + reichhaltiger State-Stream ab #1** — sonst werden Cues und Logging zum teuren Retrofit.
- ⚠️ **Schreiben (~210 h) ist nicht im Prototyp-Budget** und konkurriert im selben Fenster. Früh anfangen (ab September, während gebaut wird).
- ⚠️ **P0-Bugs + Spieler-Register** sind auf dem kritischen Pfad — ohne Register keine individuelle Rover-Steuerung, kein Per-Spieler-Logging, keine Coupling-Attribution.
- ⚠️ **~55 h/Woche Prototyp über Monate** ist beast-tauglich, aber Dauerlast — plane bewusst Regeneration ein, damit der November (Eval + Schreiben zugleich) nicht auf Reserve läuft.

