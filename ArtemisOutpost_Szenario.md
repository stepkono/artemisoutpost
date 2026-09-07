# ArtemisOutpost — Szenario (Spieldesign)

> **Zweck dieses Dokuments:** das **komplette, konkrete Spiel-Szenario** des Mondspiels — der gesamte
> Spielablauf mit allen Feinheiten der Minispiele. Es beschreibt, *was* die Teilnehmenden erleben und tun
> und *wodurch* Kollaboration/Kopplung entsteht. **Wie** etwas technisch im Code umgesetzt wird, steht
> **nicht** hier, sondern in der Technischen Skizze.
> Stand: 22. Juli 2026.

---

## 1. Studienziel & Einordnung

Das Mondspiel ist zuallererst ein **Forschungsprototyp für eine Studie zu Transitional Collaboration / Coupling Styles**: Vier Personen arbeiten über verschiedene Geräte (PC, AR am Touchtisch, VR) an einer gemeinsamen Aufgabe, und das System erfasst, wie sie sich zwischen Geräten, Ansichten und Kooperationsformen bewegen und miteinander koppeln. **Das ist das Herzstück und der eigentliche Zweck des Prototyps.**

Die **Awareness Cues** der Bachelorarbeit sind **nicht** das Herzstück, sondern ein **paralleler Entwicklungsast**, der *auf* diesem Szenario und seinen Minispielen aufsetzt — eine zusätzliche UI-/Interaktionsschicht, die untersucht, wie sich kollaboratives Verhalten in transitional interfaces durch Awareness Cues für Workspace Awareness unterstützen lässt. Beides muss umgesetzt werden, aber das Szenario steht für sich; die Cues kommen obendrauf (siehe §14).

Wichtig ist dabei: **Ein Teil von Awareness entsteht bereits durch das Spieldesign selbst.** Der Fog of War ist z. B. gleichzeitig eine Spielmechanik *und* ein Feedthrough-Signal: Ein aufgedecktes Gebiet kommuniziert allen Mitspielenden ohne ein Wort, dass dieser Bereich bereits erkundet wurde. Solche Artefakt-Zustandsänderungen tragen Awareness bereits im Kern des Spiels — die Cues der BA ergänzen das gezielt.

---

## 2. Geräte-Ebenen und ihre Rollen

Die Meta-Aufteilung der Geräte ist bewusst klar, weil sie im Paper leicht zu erklären ist: Nicht jede Aufgabe braucht eine eigene Begründung — jedes Gerät hat eine übergeordnete Rolle.

| Ebene | Rolle | Kurz |
|---|---|---|
| **PC** | Individuelle Rover-Steuerung + Daten-/Analyse-/Kontrollebene (Lenses) | **Oversight / Control** |
| **AR / Touchtisch** | Gemeinsame Übersicht, Koordination, Awareness; hier kommt man zusammen, teilt mentale Modelle, brainstormt. Zeigt den **kompletten Ist-Zustand** des Spiels. | **Zusammenkommen / Abstimmen** |
| **VR / HMD** | Handlungsebene: physische Aktionen (scannen, kartografieren, platzieren, ausrichten, aktivieren) | **Action** |

Die semantische Trennung ist strikt: In **VR passiert die Handlung**, der **PC überwacht und steuert**, die **AR ist der gemeinsame Denk- und Koordinationsraum**.

### AR-Ansicht (Touchtisch + HMDs)
- Der Touchtisch zeigt einen Kartenausschnitt der Mondoberfläche, navigierbar durch Skalieren und Verschieben. Er ist das Steuerungselement der gemeinsamen AR-Ansicht.
- Die HMDs legen denselben Kartenausschnitt als holografische Darstellung über den physischen Touchtisch; alle sehen dasselbe Bild an derselben physischen Stelle.
- In der AR sichtbar: Rover, Astronauten-Avatare, Gebäude (Habitate, Solarpanels, Funkmasten), Einflussbereiche, ggf. Scanbereiche/Missionsmarker, sowie der Fog of War.
- **Ressourcen sind in der AR bewusst *nicht* sichtbar** — diese Information lebt nur in den individuellen PC-Ansichten.

### PC-Ansicht
- Navigierbare Mondkarte mit der **Live-Position des eigenen Rovers** (fremde Rover werden nicht angezeigt).
- Steuerung des eigenen Rovers.
- **Lenses / Filtermasken** über der Karte:
  - **Ressourcen-Lens** — zeigt Ressourcen(-Mittelpunkte) in aufgedeckten Regionen.
  - **Buildability-Lens** — rot/grüne Maske, wo gebaut werden darf.
  - **Habitat-Einflussradius-Lens** — Einflussbereiche der Habitate (neues Habitat nur innerhalb eines bestehenden Bereichs, siehe §9).
  - **Signal-Lens** — Signal/Ausrichtung zwischen Funkmasten und Habitaten.
- Der noch nicht aufgedeckte Teil der Karte ist mit Fog of War verdeckt.

---

## 3. Spieler- und Besitzstruktur

- Ausgelegt auf **vier Teilnehmende**, lauffähig mit **mindestens drei** (die höchste Personen-Anforderung eines einzelnen Minispiels liegt bei drei — siehe Funkmast, §8.7).
- Jede Person besitzt **einen Rover, einen Astronauten-Avatar, einen PC-Client und einen XR-Client (Headset)**.
- **Besitz ist fest zugeordnet:** Zu einer Person gehören genau ein Headset und ein PC-Platz; dieser PC und dieses Headset sind „ihr" Gerät. Insbesondere steuert **jeder PC-Client nur seinen eigenen Rover** — man kann von einem PC aus nicht fremde Rover fahren.
- Steigt eine Person am PC in VR (steigt „aus dem Rover"), erscheint ihr Avatar an der Position ihres eigenen Rovers.

---

## 4. Rollen & Aufgaben-Slots

**Finale Entscheidungen:**
- **Keine festen Rollen.** Alle vier sind gleichwertige Astronaut:innen; jede Person kann prinzipiell alles.
- **Keine exklusiven Werkzeug-Items.**
- **Kein Task-Claiming / kein „Einloggen"** auf Aufgaben — jede Person hat Zugriff auf jede Aufgabe.
- **Aber: Slots pro Aufgabe.** Eine Aufgabe hat eine feste Zahl an VR-Teilnahme-Slots. Sind sie belegt (z. B. zwei Personen kartografieren bereits eine Ader), kann keine weitere Person in VR in dieselbe Aufgabe einsteigen. Slots je Aufgabe: Scannen 1 · Ader kartografieren 2 · Abbau (Vollversion) 1–2 · Solarpanel 1 · Habitat 2 · Funkmast 1 (+1 optional).

Kollaboration entsteht damit nicht durch Rollen, sondern durch: (1) **geräteabhängige Informationsverteilung**, (2) **Aufgaben, die mehrere Personen erfordern**, (3) **räumliche Verteilung der Rover**, (4) **zeitliche Abhängigkeiten im Ablauf**, (5) **aktive Kommunikation zwischen PC, AR und VR**.

> **Hinweis zur Asynchronität:** wird bewusst *nicht* stark als Designziel verfolgt. Die Coupling Styles bilden ab, mit wem jemand *lose* oder *eng* gekoppelt arbeitet — nicht echte Asynchronität. In einer 60–90-Minuten-Studie im selben Raum wäre echte Asynchronität ohnehin schwer plausibel. Kopplung wird über die Aufgabenstruktur erzeugt, nicht über asynchrone Rollen.

---

## 5. Narrative Rahmung

Die Teilnehmenden sind Astronaut:innen mit eigenen Rovern auf dem Mond. Am PC „sitzen" sie metaphorisch im Rover und fahren durch die Landschaft. Beim Wechsel zu VR „steigen sie aus dem Rover aus" und erscheinen als Avatar neben ihrem Rover (in der AR-Ansicht sichtbar). Dadurch wird der Reality-Wechsel erzählerisch verständlich.

> **Auftrag an die Teilnehmenden:** *Ihr seid ein vierköpfiges Astronautenteam. Baut auf dem Mond eine wachsende Habitat-Kette auf: erkundet neue Gebiete, findet Ressourcen, baut Regolith ab, errichtet Habitate und aktiviert sie über Funkmasten. Nur aktivierte Habitate zählen und erweitern euren Einflussbereich.*

---

## 6. Startzustand

- Es gibt eine kleine vorhandene **Startbasis** als Spawn-Punkt, die visuell bereits ein kleines Habitat, ein Solarpanel, einen Funkmast und einen Rover-Bereich enthalten kann.
- Um die Basis herum ist ein kleiner Kartenbereich bereits aufgedeckt (Startkreis), außen herum Fog of War.
- Das Start-Habitat zählt bereits als **Score-Basis** und sät den ersten Einflussradius, an den die Expansion andockt (§9).
- **Wichtig für VR:** Die Welt ist vor dem Scannen nicht schwarz. Die Mondoberfläche bleibt sichtbar; das Scannen schaltet Karten-/Ressourcen-/Buildability-*Daten* frei, nicht die physische Welt selbst.

---

## 7. Kern-Loop

1. Gebiet erkunden → 2. Umgebung scannen → 3. Ressource(-Mittelpunkt) finden → 4. Ressourcenader kartografieren (2 Personen VR) → 5. Ressource abbauen (VR + PC-Guidance) → 6. Solarpanel bauen + ausrichten → 7. Habitat bauen (2 Personen VR, gekoppelt) → 8. Funkmast bauen + zur Erde und zum Habitat ausrichten → 9. Habitat aktivieren → 10. Einflussbereich erweitern → 11. nächsten Standort erschließen.

(Die Cut-Version verkürzt diesen Loop — siehe §13.)

---

## 8. Phasen im Detail

### 8.1 Exploration mit Rover
Jede Person fährt mit ihrem eigenen Rover — primär am **PC**, alternativ auch aus der **AR** heraus steuerbar — vom Startgebiet in verschiedene Richtungen, um neue Gebiete zu erkunden. Die PC-Ansicht ist individuell (eigener Rover + Navigations-/Datenvisualisierung). Räumliche Verteilung der Rover erzeugt bereits lose Kopplung.

### 8.2 Umgebung scannen
Wer ein neues Gebiet erschließen will, fährt hin, stoppt und wechselt in VR („steigt aus"). In VR erscheint die Person neben ihrem Rover und führt einen **Umgebungsscan** aus, der einen Radius um sie herum aufdeckt.

- **Was der Scan aufdeckt:** den neuen Kartenbereich (für alle: AR + alle PCs), die dortigen **Terrain-/Buildability-Informationen** und die **Existenz von Ressourcen in Form ihrer Mittelpunkte**. Vor dem Scan ist alles No Man's Land unter Fog of War.
- Der Scan ist ein **bedeutungsvoller, zeitlich ausgedehnter VR-Moment** (kein simpler Klick). Die exakte VR-Geste ist in der Vollversion noch offenes Feindesign; in Plan B ist der Scan ein **5-Sekunden-Langdruck** (Dwell), damit er temporal bleibt (§13).
- 1 VR-Slot.

### 8.3 Ressource erkennen & Ader kartografieren (2 Personen VR)
Nach dem Scan sind **Ressourcen-Mittelpunkte** sichtbar (nur in der PC-Ressourcen-Lens, nicht in AR). Ressourcen sind keine Einzelpunkte, sondern **Adern/Verläufe**.

- **Zwingend zwei Personen in VR (2 Slots):** Sichtbar ist zunächst nur der **Mittelpunkt** — der Punkt, ab dem zwei Spieler:innen **in verschiedene Richtungen auseinanderlaufen**, um die Ausdehnung/Form der Ader zu erfassen. Eine Person allein kann den Verlauf nicht kartografieren; die Zwei-Personen-Bewegung entlang der Ader *ist* die Mechanik.
- **PC↔VR-Guidance entsteht implizit:** Weil Ressourceninformationen (Mittelpunkt, Verlauf, Buildability) **nur am PC** sichtbar sind, muss eine Person am PC den VR-Personen sagen, wo der Mittelpunkt liegt und in welche Richtungen zu laufen ist. Der Cross-Reality-Kopplungsmoment „PC leitet VR an" ist damit über die Lens-Asymmetrie fest eingebaut — auch ohne separaten Abbau-Schritt.

### 8.4 Ressource abbauen (Vollversion)
Nach dem Kartografieren wird die Ressource — vor allem **Regolith** — abgebaut. Die VR-Person führt den Abbau physisch aus; eine PC-Person sieht die genauen Verlaufsdaten und gibt Navigations-/Abbauhinweise. Es gibt **kein individuelles Inventar**, sondern ein **geteiltes Team-Inventar**: Sobald abgebaut, ist die Einheit für alle verfügbar.

> **Allgemeine Mechanik (beide Versionen):** Sobald eine Ressource gewonnen ist, **verschwindet ihr Mittelpunkt** (und die Ader) aus der Kartendarstellung — betrifft v. a. die PC-Ansicht/Lens. Das ist semantisch logisch: eine abgebaute Ressource wird nicht mehr angezeigt. In der **Vollversion** geschieht das nach dem Abbau, in **Plan B** bereits nach dem vollständigen Kartografieren (§13).

### 8.5 Solarpanel bauen (Vollversion, Ein-Personen-Task)
Aus Regolith kann eine **einzelne** Person in VR ein Solarpanel platzieren und zur Sonne ausrichten (Effizienz abhängig vom Winkel). Das Panel erzeugt danach **Energie** für weitere Bauaktionen. Diese Aufgabe ist bewusst eine kleine Einzelaufgabe (1 Slot), damit es neben den großen Gruppenaufgaben auch Solo-Momente gibt. Später kann eine Nachjustierung nötig werden.

### 8.6 Habitat bauen (2 Personen VR, eng gekoppelt)
Das Habitat ist das zentrale Score-Objekt. Voraussetzungen: Regolith (+ Energie in der Vollversion), eine gültige Baufläche (Buildability-Zone, per Buildability-Lens am PC einsehbar) und **mindestens zwei Personen in VR**. Ein neues Habitat darf nur **innerhalb** des Einflussradius eines bestehenden Habitats liegen (§9).

**Primärmechanik — Zwei-Achsen-Nivellierung (empfohlen).**
Das Fundament erscheint an einer bereits gültigen Position (die Position ist hier *nicht* die Herausforderung — die regelt die Buildability-Zone). Die gekoppelte Herausforderung ist das **Nivellieren des Fundaments**: **Person A steuert Pitch, Person B steuert Roll**, jeweils gedämpft auf einen Controller gemappt. Ein **geteiltes „Wasserwaagen"-Feedback** (eine Blase, die zentriert werden muss) zeigt beiden denselben gemeinsamen Zustand. Keine Person kann die Achse der anderen korrigieren → beide müssen gleichzeitig agieren. Platziert wird erst, wenn **beide Achsen zugleich** für ~2–3 s in Toleranz sind (driftet eine heraus, Reset). Optional als zusätzliche Kopplung: beide halten im finalen Zustand gleichzeitig einen „Commit"-Trigger.

*(Diese Mechanik ersetzt bewusst die ursprünglich angedachte „Tauziehen"-Physik mit geteiltem Greifen — die ist für eine Studie schwerer zuverlässig zu bekommen. Die deterministische Pitch/Roll-Nivellierung koppelt genauso zwingend zwei Personen und ist robuster.)*

Den **Zwei-Griff-Fallback** siehe Plan B (§13).

### 8.7 Funkmast bauen & Habitat aktivieren
Ein Habitat zählt erst zum Score, wenn es **aktiviert** wurde — dafür braucht es einen Funkmast (Baukosten: Regolith, in der Vollversion + Energie, jeweils weniger als ein Habitat). Dies ist das **stärkste Transitional-Interface-Szenario**, weil PC, AR und VR gleichzeitig gebraucht werden. Es sind zwei Ausrichtungen nötig:

| Ebene | temporäre Aufgabenfunktion |
|---|---|
| **PC-Person** | sieht die Signal-/**Erd**-Ausrichtung und sagt an, wie der Funkmast zur Erde rotiert werden muss |
| **AR-Person (Touchtisch)** | sieht Habitat und Funkmast in der Übersicht und sagt die Richtung/Verbindung **zum Habitat** an |
| **VR-Person** | manipuliert und rotiert den Funkmast physisch (1 Slot) |
| **optionale 2. VR-Person** | übernimmt parallel die zweite Ausrichtung (eine Person Erde, eine Person Habitat) — +1 Slot |

- **Erd-Ausrichtung variiert jedes Mal**, ist also nicht auswendig lernbar; die Aufgabe bleibt bei jeder Wiederholung frisch. (Die „Erde" ist erzählerischer Hintergrund; die konkrete technische Umsetzung der Richtung gehört in die Skizze.)
- Nach erfolgreicher Ausrichtung beider Achsen wird das Habitat **aktiviert**, der Score steigt, der Einflussbereich erweitert sich.
- **Konvergenz ist gewollt:** Dass sich am Funkmast bis zu vier Personen sammeln, während sonst verteilt exploriert wird, ist ausdrücklich Teil des Spiels und Entscheidung der Gruppe — der Rhythmuswechsel „alle zusammen ↔ verteilt" ist für die Kopplungsdaten sogar interessant.

---

## 9. Score & Expansion

- **Nur aktivierte Habitate zählen** zum Score. Das Start-Habitat ist die Score-Basis.
- Jedes aktivierte Habitat erzeugt einen **Einflussradius**.
- **Expansionsregel:** Ein neues Habitat ist nur gültig, wenn es **innerhalb** des Einflussradius eines bestehenden Habitats liegt. (Diese „innerhalb"-Regel dämpft die Expansion bewusst — weniger aggressiv, als wenn sich nur die Radien überschneiden müssten.) Nach Aktivierung erweitert das neue Habitat den erreichbaren Bereich, wodurch weiter entfernte Bauorte möglich werden.
- **Rundenende:** über ein **Zeitlimit** (60–90 min, intern noch festzulegen). **Kein Ziel-Score** — die Zeit beendet die Runde.

---

## 10. Views, Lenses & kontextabhängige UI

- Die Teilnehmenden öffnen nicht ständig neue Interfaces, sondern nutzen die **Lens-/Filter-Logik** (§2, PC-Ansicht).
- **Kontextabhängige UI:** Startet eine VR-Person eine Aufgabe, wird ein Event an den Server gesendet und an die PC-Clients verteilt; dort werden die passenden Daten der korrekten geodätischen Position und dem korrekten Spieler zugeordnet.
- **Informationsstand:** **Alle PC-Clients haben aktuell denselben Informationsstand** — kein PC-Client hat exklusive Daten, es muss sich niemand einloggen oder eine Aufgabe „claimen". (Ownership betrifft nur den eigenen Rover, §3.)
- **Mögliche spätere Entwicklung (Vermerk):** Es kann im Laufe der Entwicklung noch entschieden werden, ob es abgrenzende Lenses zwischen PC-Ansichten gibt — also ob manche PC-Clients an die Player-ID gekoppelte, unterschiedliche Informationen sehen. **Starke Tendenz: alle PC-Clients gleich.** Die player-ID-gebundene Variante bleibt als Option notiert.

---

## 11. Studienraum & Rahmenbedingungen

- Die vier sind **physisch im selben Raum** und **reden laut miteinander**. Der Raum wird über **Raum-Mikrofone** aufgenommen (der gesamte Raum, **keine** Mikros pro Person).
- **Keine Blackscreen-Mechanik.** Unerwünschtes Verhalten (an fremde PCs gehen, unter der Brille auf fremde Monitore schauen) wird **durch Design minimiert** statt technisch erzwungen: Der eigene PC zeigt primär den eigenen Rover und taskspezifische Daten; task-relevante Informationen sind gezielt an Ansichten gebunden; VR-Aktionen sind nur in Nähe des eigenen Rovers möglich; AR bleibt die explizite gemeinsame Koordinationsfläche. Die Studienleitung beobachtet und greift bei undefinierten Zuständen ein.
- **Robustheit:** Das gesamte Szenario funktioniert auch mit **drei** Teilnehmenden (die maximale Personen-Anforderung eines Minispiels ist drei).

---

## 12. Kompakter End-to-End-Ablauf (Vollversion)

1. Alle starten an der Basis, jede Person steuert ihren Rover am eigenen PC.
2. Eine Person fährt in ein unbekanntes Gebiet, stoppt, wechselt in VR.
3. In VR scannt sie die Umgebung → Bereich + Buildability + Ressourcen-Mittelpunkte werden auf PC und AR freigeschaltet.
4. Eine Ressource wird sichtbar (Mittelpunkt, nur in der PC-Lens).
5. Zwei Personen fahren hin, steigen in VR aus und **kartografieren die Ader**, indem sie vom Mittelpunkt in verschiedene Richtungen auseinanderlaufen; eine PC-Person leitet sie dabei an.
6. Eine VR-Person **baut die Ressource ab**, angeleitet vom PC → Team-Inventar erhält Regolith; der Mittelpunkt verschwindet von der Karte.
7. Eine Person baut in VR ein **Solarpanel** und richtet es zur Sonne aus → Team erhält Energie.
8. Zwei Personen bauen gemeinsam in VR ein **Habitat** (Pitch/Roll-Nivellierung) an einer gültigen, im Einflussbereich liegenden Position.
9. Ein **Funkmast** wird gebaut; PC (Erde), AR (Habitat) und VR (Manipulation) richten ihn gemeinsam aus.
10. Das Habitat wird **aktiviert**, der Score steigt, der Einflussradius wächst.
11. Das Team wiederholt den Prozess für das nächste Habitat, bis das Zeitlimit die Runde beendet.

---

## 13. Plan B (Cut-Version)

Eine bewusst reduzierte, de-riskte Variante als Absicherung. Sie unterscheidet sich vom vollen Szenario in genau vier Punkten:

1. **Solarpanel & Energie komplett raus.** Es gibt keine Energie-Ökonomie mehr. Gebäude kosten nur noch **Regolith**; es geht allein darum, Ressourcen zu haben und bauen zu können. Habitate werden weiterhin **ausschließlich über den Funkmast aktiviert**.
2. **Ressourcen-Abbau raus.** Es gibt keinen separaten Abbau-Schritt mehr — **das vollständige Kartografieren der Ader erzeugt bereits die Ressource** (Regolith ins Team-Inventar). Danach verschwindet der Mittelpunkt von der Karte (wie in §8.4 beschrieben, hier nur früher im Ablauf). Die Zwei-Personen-Kartografierung und die implizite PC-Guidance bleiben vollständig erhalten.
3. **Scan = 5-Sekunden-Langdruck (Dwell)** statt einer freien VR-Geste — hält die Interaktion temporal und bedeutungsvoll, ohne aufwändiges Gesten-Design.
4. **Habitat-Platzierung = Zwei-Griff-Fallback.** Statt der Pitch/Roll-Nivellierung hat das Fundament zwei Griffe auf gegenüberliegenden Seiten. Jede der zwei VR-Personen greift einen; platziert wird nur, wenn **beide gleichzeitig** ihren Griff halten und innerhalb eines kurzen Fensters gemeinsam auslösen — innerhalb der Buildability-Zone. **Hier ist die genaue Position Teil der gekoppelten Aufgabe** (anders als in der Primärmechanik, wo die Position automatisch gesetzt wird und nur das Nivellieren gekoppelt ist). Koppelt über räumliche Verteilung (beide müssen sich gegenüber positionieren) + Synchronisation statt über Feinrotation — noch simpler und robuster, dafür ohne den Ausrichtungs-Reiz.

Alles Übrige (Geräte-Rollen, Exploration, Scan-Reveal-Logik, Ader-Kartografierung durch zwei Personen, Funkmast als PC-AR-VR-Aufgabe, Score & Expansion, Studienraum-Regeln) bleibt identisch zur Vollversion.

---

## 14. Awareness-Cues — Einordnung (paralleler Ast)

Die Awareness Cues sind **kein** Teil des Spiel-Szenarios im engeren Sinn, sondern eine **zusätzliche Schicht**, die für die Bachelorarbeit *auf* dem fertigen Szenario und seinen Minispielen implementiert wird. Welche konkreten Cues das sein werden, ist an dieser Stelle bewusst offen und gehört nicht in die Szenario-Beschreibung.

Festzuhalten ist nur: **Ein Teil dessen, was Awareness Cues leisten, wird bereits durch das Spieldesign selbst abgedeckt** — etwa VR-Char-Repräsentation, Blickrichtung und Artefakt-Feedthrough. Der **Fog of War** ist das klarste Beispiel: gleichzeitig Spielmechanik *und* ein Artefakt, das durch seinen Zustandswechsel („dieses Gebiet ist erkundet") ohne verbale Kommunikation an alle kommuniziert. Auf dieser bereits im Spiel angelegten Awareness-Substanz setzen die BA-Cues gezielt auf.

---

## 15. Offene Entscheidungen

| Thema | Status |
|---|---|
| Konkretes **Zeitlimit** (60–90 min) | intern noch festzulegen |
| **VR-Scan-Geste** der Vollversion (exakte Form) | offenes Feindesign |
| **Player-ID-gebundene Lenses** (unterschiedliche PC-Infos) | offen; starke Tendenz zu „alle PC gleich", Variante als Option notiert |
| Habitat: Primärmechanik vs. Fallback im echten Test | Pitch/Roll primär; Zwei-Griff-Fallback in Plan B als Absicherung |
