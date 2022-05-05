# LokLiftController

Eine Arduino Steuerungs-Box für einen Lok Lift – eine Lift-System für Modell-Eisenbahnen.
Die Box ist auf einem Arduino Mega aufgebaut und hat folgende Features:

- 12 Positionen speicherbar
- einfaches Speichern der Gleis-Positionen
- per Drehschalter ist der Motor direkt steuerbar
- zwei verschiedene Motor-Modi: schnelle Motor-Bewegung und Feinjustierung
- einfache externe Anbindung der 12 Positionsschalter (z.B. für eine Computer-Steuerung)

[BILD der Box mit den Schaltern]
[BILD der Box mit den Terminals]

## Erster Start

Nachdem die Box mit Strom versorgt wird (5-12V für den Arduino Mega über Netzteil oder über USB) wird der Start-Bildschirm angezeigt.

[BILD]

Hier hat man 5 Sekunden Zeit, um die Streckenmessung zu starten. Der Ablauf der 5 Sekunden wird duch die Punkte visualisiert.
Drückt man während dieser 5 Sekunden auf den roten Speicher-Schalter startet man die Streckenmessung.

## Streckenmessung

Dieser Vorgang ist wichtig und sollte als erstes einmalig durchgeführt werden. Hierbei fährt der Motor die beiden Endstopps an
und speichert die gefahrene Strecke ab. Dies ist notwendig, um später die Positionen genau anfahren zu können.

Wenn die Streckenmessung startet fährt der Motor zunächst zum Endstopp B (Motor-Drehrichtung im Uhrzeigersinn).
Sobald Endstopp B erreicht ist wechselt die Drehrichtung (entgegen Uhrzeigersinn) und fährt zurück bis Endstopp A ausgelöst wird.
Bitte achte bei deinem Aufbau entsprechend darauf, dass die Endstopps entsprechend angeordnet sind, dass genau die Abfolge wie beschrieben durchgeführt werden kann.

Nachdem am Ende der Streckenmessung Enstopp A erreicht wurde, setzt der Motor wieder kurz zurück, damit der Endstopp A nicht dauerhaft 
ausgelöst wird, und die Wegstrecke wird gespeichert. Die Streckenmessung ist nun abgeschlossen und die Box geht in den normalen Betrieb über.
Das Display gibt eine entsprechende Meldung aus.

[BILD]

## Normaler Betrieb (ohne Streckenmessung)

Wenn innerhalb der 5 Sekunden im Start-Bildschirm nichts gedrückt wird, fährt der Motor zunächst Endstopp A einmal an, damit der Motor weiß, 
wo er sich befindet. Im Display erscheint dann "Kalibrierung ..."

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

Mit dem Drehregler können Sie den Motor manuell bewegen. Nach dem Start der Box und sobald "Bahn frei!" auf dem Display erscheint, ist die 
Motor-Steuerung im sog. "Lauf-Modus". Dieser ist für eine schnelle (grobe) Einstellung gedacht. Mit dem Drehregler steuert man die 
Geschwindigkeit des Motors. Der Motor läuft immer weiter, bis er gestoppt wird. Sollte der Motor einen Endstopp auslösen, wird die Richtung 
des Motors geändert.

Um den Motor anzuhalten kann man entweder den Drehregler nutzen, um die Geschwindigkeit bis auf Null zu verlangsamen, oder einfach auf den 
Drehregler drücken. Das Drücken stoppt den Motor sofort und wechselt die Motor-Steuerung in den "Schritt-Modus". In der Regel ist dies auch 
gewünscht, da man nach der groben Anfahrt in die Feinjustierung wechseln möchte. Falls dies nicht gewünscht ist, einfach ein weiteres mal auf 
den Drehregler drücken, dann ist der "Lauf-Modus" wieder aktiv. Im Display wird der aktuelle Modus auch kurz dargestellt.

[BILD]
[BILD]

Wenn man den Motor-Modus wechselt, wird die Geschwindigkeit immer zurückgesetzt, damit dieser nicht automatisch los läuft. D.h. die 
Geschwidniigkeit ist nach dem wechseln immer Null. Wie oben beschrieben kann die Geschwindigkeit nach dem Modus-Wechsel mit dem Drehregler 
gesteuert werden.

#### Schritt-Modus
Im "Schritt-Modus" wird mit jedem Schritt des Drehreglers ein einzelner Motor-Schritt gefahren. So können die Gleise sehr genau ausgerichtet werden.

## Positionen speichern

Um nun die Position zu speichern drücke einmal auf den Speichern-Schalter. So weiss die Box, dass die Position nun gespeichert werden soll.
Anschließend drücke einen der 12 Positions-Schalter, um die Position auf diesen Schalter zu legen.

[BILD]

Falls das Speichern abgebrochen werden soll, einfach nochmal den Speichern-Schalter drücken.

[BILD]
