# LokLiftController

Eine Arduino Steuerungs-Box für einen Lok Lift – ein Lift-System für Modell-Eisenbahnen.

Die Box ist auf einem Arduino Mega aufgebaut und hat folgende Features:

- 12 Positionen speicherbar
- einfaches Speichern der Gleis-Positionen
- per Drehschalter ist der Motor direkt steuerbar
- zwei verschiedene Motor-Modi: schnelle Motor-Bewegung und Feinjustierung
- einfache externe Anbindung der 12 Positionsschalter (z.B. für eine Computer-Steuerung)

[BILD der Box mit den Schaltern]
[BILD der Box mit den Terminals]

**ACHTUNG: wenn die EndStops ausgelöst werden, muss der Motor abrupt stoppen bevor die Richtung gewechselt wird. Möglicherweise können durch das abrupte Stoppen Züge aus den Gleisen geworfen werden. Ich empfehle die Streckenmessung und das Einstellen der Positionen lieber zunächst ohne Züge einzustellen. Oder probiert es zunächst nur mit einem Zug testweise aus.**

**Wenn du auf Nummer sicher gehen willst, lese erst die komplette Anleitung und stelle alle benötigten Positionen ein (Speichern nicht vergessen!). Erst danach solltest du die Gleise mit Zügen bestücken. Beim normalen Anfahren der abgespeicherten Positionen, sollten die Endstopps nie ausgelöst werden.**

**ACHTUNG: Wenn die Controller-Box aus- und wieder angeschaltet wird, wird einmal der Endsstopp A angefahren (Kalibrations-Fahrt), damit die Controller-Box weiß, wo der Lift gerade steht. Wenn das ein Problem sein sollte (abrupter Stopp am Endstopp), dann entweder die Box immer laufen lassen. Und/Oder die Geschwindigkeit der Kalibarations-Fahrt verringern (im Einstellungs-Menu, der Punk CalRPM**

## Erster Start

Nachdem die Box mit Strom versorgt wird (7-12V für den Arduino Mega über Netzteil Hohlbuchse oder 5V über USB) wird der Start-Bildschirm angezeigt.

[BILD]

Im Start-Bildschirm hat man 5 Sekunden Zeit, um die Streckenmessung zu starten. Der Ablauf der 5 Sekunden wird duch die Punkte visualisiert.
Drückt man während dieser 5 Sekunden auf den roten Speicher-Schalter startet man die Streckenmessung.

## Streckenmessung

Dieser Vorgang ist wichtig und sollte als erstes einmalig durchgeführt werden. Hierbei fährt der Motor die beiden Endstopps an
und speichert die gefahrene Strecke ab. Dies ist notwendig, um später die Positionen genau anfahren zu können.

Wenn die Streckenmessung startet fährt der Motor zunächst zum Endstopp B (Motor-Drehrichtung im Uhrzeigersinn).
Sobald Endstopp B erreicht ist wechselt die Drehrichtung (entgegen Uhrzeigersinn) und fährt zurück bis Endstopp A ausgelöst wird.
Bitte achte bei deinem Aufbau darauf, dass die Endstopps entsprechend angeordnet sind, so dass genau die Abfolge wie beschrieben durchgeführt werden kann.

[SKIZZE AUFBAU]

Nachdem am Ende der Streckenmessung Endstopp A erreicht wurde, setzt der Motor wieder kurz zurück, damit der Endstopp A nicht dauerhaft 
ausgelöst wird. Die gemessene Wegstrecke wird automatisch gespeichert. Die Streckenmessung ist nun abgeschlossen und die Box geht in den normalen Betrieb über.
Das Display gibt eine entsprechende Meldung aus.

[BILD]

## Normaler Betrieb (ohne Streckenmessung)

Wenn innerhalb der 5 Sekunden im Start-Bildschirm nichts gedrückt wird, fährt der Motor zunächst Endstopp A einmal an, damit der Controller weiß, 
wo sich der Lift befindet. Im Display erscheint dann "Kalibrierung ..."

[BILD]

Sobald "Bahn frei!" im Display erscheint, kann die Box normal verwendet werden, d.h. also Positionen über die 12 Positions-Schalter anfahren 
oder neue Positionen abspeichern.

## Positionen anfahren

Wenn auf den 12 Schaltern Positionen gespeichert sind, einfach den entsprechenden Schalter drücken, um die Position anzufahren. Während des 
Anfahrens sind keinen weiteren Aktionen möglich.

Wurde auf dem Schalter noch keine Position gespeichert, erscheint nur eine entsprechende Meldung im Display.

[BILD]

## Motor ausrichten

#### Lauf-Modus

Mit dem Drehregler kannst du den Motor manuell bewegen. Nach dem Start der Box und sobald "Bahn frei!" auf dem Display erscheint, ist die 
Motor-Steuerung im sog. "Lauf-Modus". Dieser ist für eine schnelle (grobe) Einstellung gedacht. Mit dem Drehregler steuert man die 
Geschwindigkeit des Motors. Der Motor läuft immer weiter, bis er gestoppt wird. Sollte der Motor einen Endstopp auslösen, wird die Richtung 
des Motors geändert.

Um den Motor anzuhalten kann man entweder den Drehregler nutzen, um die Geschwindigkeit bis auf Null zu verlangsamen, oder einfach auf den 
Drehregler drücken. Das Drücken stoppt den Motor sofort und wechselt die Motor-Steuerung in den "Schritt-Modus". In der Regel ist dies auch 
gewünscht, da man nach der groben Anfahrt in die Feinjustierung wechseln möchte. Falls dies nicht gewünscht ist, einfach ein weiteres Mal auf 
den Drehregler drücken, dann ist der "Lauf-Modus" wieder aktiv. Im Display wird der aktuelle Modus auch kurz dargestellt.

[BILD]
[BILD]

Wenn man den Motor-Modus wechselt, wird die Geschwindigkeit immer zurückgesetzt, damit dieser nicht automatisch losläuft. D.h. die 
Geschwindigkeit ist nach dem Modus-Wechsel immer Null. Wie oben beschrieben kann die Geschwindigkeit nach dem Modus-Wechsel mit dem Drehregler 
gesteuert werden.

#### Schritt-Modus
Im "Schritt-Modus" wird mit jedem Schritt des Drehreglers ein einzelner Motor-Schritt gefahren. So können die Gleise sehr genau ausgerichtet werden.

## Positionen speichern

Um nun die Position zu speichern drücke einmal auf den Speichern-Schalter. So weiss die Box, dass die Position nun gespeichert werden soll.
Anschließend drücke einen der 12 Positions-Schalter, um die Position auf diesen Schalter zu legen.

[BILD]

Falls das Speichern abgebrochen werden soll, einfach nochmal den Speichern-Schalter drücken.

[BILD]
