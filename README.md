# LokLiftController

![](docs/images/LokLift-Controller-01.jpg)

Eine Arduino Steuerungs-Box für einen Lok Lift – ein Lift-System für Modell-Eisenbahnen.

Die Box ist auf einem Arduino Mega aufgebaut und hat folgende Features:

- 12 Positionen speicherbar
- einfaches Speichern der Gleis-Positionen
- per Drehschalter ist der Motor direkt steuerbar
- zwei verschiedene Motor-Modi: schnelle Motor-Bewegung und Feinjustierung
- einfache externe Anbindung der 12 Positionsschalter (z.B. für eine Computer-Steuerung)

<img src="docs/images/LokLift-Controller-01.jpg" width="350"><img src="docs/images/LokLift-Controller-02.jpg" width="350">

***ACHTUNG:*** wenn die Endstops ausgelöst werden, muss der Motor abrupt stoppen bevor die Richtung gewechselt wird. Möglicherweise können durch das abrupte Stoppen Züge aus den Gleisen geworfen werden. Ich empfehle die Streckenmessung und das Einstellen der Positionen lieber zunächst ohne Züge einzustellen. Oder probiert es zunächst nur mit einem Zug testweise aus.

Wenn du auf Nummer sicher gehen willst, lese erst die komplette Anleitung und stelle alle benötigten Positionen ein (Speichern nicht vergessen!). Erst danach solltest du die Gleise mit Zügen bestücken. Beim normalen Anfahren der abgespeicherten Positionen, sollten die Endstops nie ausgelöst werden.

***ACHTUNG:*** Wenn die Controller-Box aus- und wieder angeschaltet wird, wird einmal der Endstop A angefahren (***Kalibrations-Fahrt***), damit die Controller-Box weiß, wo der Lift gerade steht. Wenn das ein Problem sein sollte (abrupter Stopp am Endstop), dann entweder die Box immer laufen lassen, damit sie keinen Kalibrations-Fahrt macht. Oder die Geschwindigkeit der Kalibrations-Fahrt verringern (im Einstellungs-Menu unter Punkt ***CalRPM***).

## Motor-Treiber anschließen

Hier Beschreibe ich, wie ich den Motor-Treiber angeschlossen habe. Wie man den Schrittmotor an den Motor-Treiber anschließt, hängt vom jeweiligen Motor-Treiber und der Art der Schritt-Motoren ab, daher gehe ich darauf nicht genauer ein. Das sollte Teil der Dokumentation zu deinem Motor-Treiber und Schrittmotoren sein. Google ist auch immer hilfreich 😉

Im folgenden Schema sieht man, wie ich mein Setup angeschlossen habe:

<img src="docs/images/LokLift-Controller-Setup.jpg">

Das Ganze ist so ausgelegt, dass du für den Motor-Treiber und für die Controller-Box jeweils ein eigenes Netzteil benötigts.
Mein Motor-Treiber benötigt zwischen 9 und 42 Volt und müssen über die beiden Terminals VCC und GND angeschlossen werden.
Ich habe ein 12 Volt Netzteil verwendet und mit einer fertigen Hohlbuchse mit Terminal das Ganze verbunden. 

<img src="docs/images/LokLift-Controller-05.jpg">

Die Controller-Box bzw. der darin verbaute Arduino Mega kann mit einem 7 bis 12 Volt Netzteil über die Hohlbuchse betrieben werden. Hier könntest du also das gleiche Netzteil verwenden. Oder mit 5 Volt über die USB-Buchse wäre auch möglich.

Hier die Links zu Netzteil und Hohlbuchse:
- [Netzteil 12 V](https://geni.us/XL8mi2)
- [Hohlbuchse 5,5 mm mit Terminal Adapter](https://geni.us/BXMjbb)


## Motor an Motor-Treiber anschließen

Während der Entwicklung habe ich einen 17HS4401 Bi-Polar Schritt-Motor verwendet. Bi-Polare Schritt-Motoren haben vier Kabel und haben normalerweise immer die gleiche Farbcodierung. Ich habe sie folgendermaßen an den Treiber angeschlossen:

| Motor    | Motor-Treiber | 
| -------- | ------------- | 
| Schwarz  | A+            |
| Grün     | A-            | 
| Rot      | B+            |
| Blau     | B-            | 

<img src="docs/images/Motor-Driver-Stepper-Motor.jpg">

***ACHTUNG:*** Natürlich musst du für einen Lok-Lift einen größeren Motor und Treiber verwenden. Wir testen demnächst diese beiden Typen. Ich werde berichten, wie sich damit der Controller schlägt:
- [Nema34 12NM Schrittmotor und Treiber](https://geni.us/hxMw)

## Endstops anschließen

Die Endstops müssen auch mit der Controller-Box verbunden werden. Achte darauf NO (normally open) Endstops zu verwenden. Meine verwendeten Endstops können sowohl NO als auch NC (normally closed). Deswegen haben diese Endstops drei Pins: einen für NO, einen für NC und einen für Ground. 

Du musst also den NO Pin von EndstopA an dem Terminal A der Controller-Box anschließen. Entsprechend den NO Pin von Ensdtopp B an Terminal B. Und beide Ground Pins der Endstops werden ebenfalls an Ground der Controller-Box angeschlossen.

<img src="docs/images/LokLift-Controller-Endstops.jpg">

Hier die Links zu meinen verwendeten Endstops:
[Endstop Mikroschalter mit Rollenhebel](https://geni.us/ZmCi)

## Erster Start

Wenn alles korrekt verkabelt ist, können wir den LokLift-Controller starten. Die Stromversorgung des Motor-Treibers würde ich zuerst herstellen. Dann die COntroller-Box mit Strom versorgen. Kuz darauf wird der Start-Bildschirm angezeigt.

<img src="docs/images/LokLift-Controller-04.jpg">

Im Start-Bildschirm hat man 5 Sekunden Zeit, um die Streckenmessung zu starten. Der Ablauf der 5 Sekunden wird duch die Punkte visualisiert.

Drückt man während dieser 5 Sekunden auf den roten Speichern-Taster (rechts neben dem grauen Dreh-Knopf) startet man die Streckenmessung.
Drückt man während dieser 5 Sekunden auf den grauen Dreh-Knopf gelangt man ins Einstellungs-Menu.

Falls du einen Schrittmotor verwendet, der keine 200 PPR bzw 1,8° pro Schritt verwendet, solltest du zunächst den MotPPR Wert anpassen. Wie das geht, findet du im Abschnitt [Einstellungs-Menu](#einstellungs-menu)

Wir wollen zunächst die Streckenmessung starten.

## Streckenmessung

Dieser Vorgang ist wichtig und muss als erstes einmalig durchgeführt werden. Sollte sich am Aufbau deines Lifts etwas ändern, muss die Streckenmessung erneut durchgeführt werden. Ausserdem sollten die gespeicherten Position erneut eingestellt und abgespeichert werden.

Bei der Streckenmessung fährt der Motor die beiden Endstops an und speichert die Länge (Schritte des Motors) der dabei gefahrenen Strecke ab. Dies ist notwendig, um später die Positionen genau anfahren zu können.

Wenn die Streckenmessung startet fährt der Motor zunächst zum Endstop B (Motor-Drehrichtung im Uhrzeigersinn, wenn man von oben auf den Schaft schaut).
Sobald Endstop B erreicht ist wechselt die Drehrichtung des Motors (entgegen Uhrzeigersinn) und fährt zurück bis Endstop A ausgelöst wird.

Bitte achte bei deinem Aufbau darauf, dass die Endstops entsprechend angeordnet sind und der Motor sich wie beschrieben dreht, so dass genau die Abfolge wie beschrieben durchgeführt wird.

Falls der Motor sich falsch dreht, schaue auch mal im Bereich ***Tipps/Fehlerbehebungen*** nach.

<img src="docs/images/LokLift-Streckenmessung.jpg">

Nachdem am Ende der Streckenmessung Endstop A erreicht wurde, setzt der Motor wieder kurz zurück, damit der Endstop A nicht dauerhaft 
ausgelöst wird. Die gemessene Wegstrecke wird automatisch gespeichert. Die Streckenmessung ist nun abgeschlossen und die Box geht in den normalen Betrieb über.

Das Display gibt eine entsprechende Meldung aus.

## Normaler Betrieb (ohne Streckenmessung)

Wenn innerhalb der 5 Sekunden im Start-Bildschirm nichts gedrückt wird bzw. nach der Streckenmessung fährt der Motor zunächst Endstop A einmal an, damit der Controller weiß, wo sich der Lift befindet. Im Display erscheint dann ***Kalibrierung ...***

<img src="docs/images/LokLift-Controller-06.jpg">


Sobald "Bahn frei!" im Display erscheint, kann die Box normal verwendet werden, d.h. also Positionen über die 12 Positions-Schalter anfahren 
oder neue Positionen abspeichern.

Darunter wird die aktuelle Position des Lifts angezeigt.

<img src="docs/images/LokLift-Controller-07.jpg">

## Positionen auf den 12 Tastern speichern

Wenn die Kalibierung abgeschlossen ist, kannst du nun beginnen, neue Positionen für deinen Lift auf die 12 Taster zu speichern. Dazu musst du zunächst die gewünschte Position anfahren. Um das zu erreichen, gibt es zwei verschiedene Motor-Modi – eine für grobes Anfahren (***Lauf-Modus***) und einen für die Feinjustierung (***Schritt-Modus***). Nach der Kalibierung ist zunächst der ***Lauf-Modus*** aktiv. 

#### Lauf-Modus

Der ***Lauf-Modus*** ist für eine schnelle grobe Einstellung gedacht. Mit dem Drehregler steuert man die 
Geschwindigkeit des Motors. Der Motor läuft immer weiter, bis er gestoppt wird. Sollte der Motor einen Endstop auslösen, wird die Richtung 
des Motors automatisch geändert.

Um den Motor anzuhalten, kann man entweder den Drehregler nutzen, um die Geschwindigkeit bis auf Null zu verlangsamen, oder einfach auf den 
Drehregler drücken. 

Das Drücken stoppt den Motor sofort. Ausserdem wechselt der Modus nun in den ***Schritt-Modus***. In der Regel ist dies auch 
erwünscht, da man nach der groben Anfahrt nun in die Feinjustierung wechseln möchte. 

Falls dies nicht gewünscht ist, einfach ein weiteres Mal auf den Drehregler drücken, dann ist der ***Lauf-Modus*** wieder aktiv. 

Im Display wird der aktuelle Modus auch kurz durch eine entsprechende Nachricht dargestellt.

Wenn man den Motor-Modus wechselt, wird die Geschwindigkeit immer zurückgesetzt, damit der Motor nicht automatisch losläuft. D.h. die 
Geschwindigkeit ist nach dem Modus-Wechsel immer Null. 

#### Schritt-Modus
Im ***Schritt-Modus*** wird mit jedem Schritt des Drehreglers ein einzelner Motor-Schritt gefahren. So können die Gleise sehr genau ausgerichtet werden.

Ein Druck auf den Dreh-Regler wechselt wieder in den ***Lauf-Modus***.

## Positionen abspeichern

Hat der Lift nun die gewünschte Position erreicht, muss die Position auf einem der 12 Positions-Taster abgespeichert werden.

Dazu drücke einmal auf den roten Speichern-Taster (rechts neben dem Dreh-Regler). So weiss die Box, dass die Position nun gespeichert werden soll.
Anschließend drücke einen der 12 Positions-Taster, um die Position auf diesen Schalter zu speichern.

<img src="docs/images/LokLift-Controller-Box.jpg">

Falls das Speichern abgebrochen werden soll, einfach nochmal den Speichern-Taster drücken.

Wenn du alle Position gespeichert hast, kannst du nun Züge auf die Gleise setzen. Mit den Positions-Tastern werden die gespeicherten Positionen sanft  angefahren. Vorsichtiges nachjustieren und erneutes abspeichern sollte mit Zügen kein Problem sein. Passe nur auf, dass du keinen der Endstops erreichst, da hier die Richtung möglicherweise zu abrupt geändert wird. Allerdings dürfte das nur beim Arbeiten mit dem ***Lauf-Modus*** passieren. Und bei der Kalibrierungsfahrt nach einem Neustart des Controllers oder bei der erstmaligen Streckenemessung.

## Positionen anfahren

Wenn auf den 12 Tastern Positionen gespeichert sind, einfach den entsprechenden Taster drücken, um die Position anzufahren. Während des 
Anfahrens sind keine weiteren Aktionen möglich, bis die gespeicherte Position erreicht wurde.

Wurde auf dem Taster noch keine Position gespeichert, erscheint nur eine entsprechende Meldung im Display.

## Einstellungs-Menu

Im Einstellungs-Menu können Motor-Parameter angepasst werden. Du hast zwei Möglichkeiten, um es zu öffnen:
- während der 5 sekündigen Startphase den Dreh-Regler drücken
- im normalen Betrieb den Dreh-Regler doppelt drücken

<img src="docs/images/LokLift-Controller-08.jpg">

Folgende Parameter können dort eingestellt werden:

| Name   | Funktion      | 
| ------ | ------------- | 
| MaxRPM | Die maximale Motorgeschwindigkeit in RPM, die der Motor beim Anfahren der Positionen oder im ***Lauf-Modus*** erreichen kann. |
| MinRPM | Die minimale Motorgeschwindigkeit in RPM. Das ist die Startgeschwinbdigkeit im ***Lauf-Modus*** | 
| CalRPM | Die Motorgeschwindigkeit in RPM, die während der Kalibrierungsfahrt eingestellt wird. |
| AccStp | eispiel: dieser Wert steht auf 200 und die neue Position, die angefahren werdne soll, ist 1000 Schritte entfernt. Dann würde während der ersten 200 Schritte ein sanftes Anfahren durchgefürt werden. Ab Schritt 201 wird die maxRPM erreicht. Ab Schritt 800 wird dann wieder sanft bis zum Ziel abgebremst. | 
| MotPPR | Hier musst du den PPR Wert deines Motors angeben. Der PPR Wert gibt, wieviele Schritte ein Motor für einen volle Umdrehung benötigt.<br>Oft findet man Motoren mit 200 PPR – das entspricht 1,8° pro Schritt. Wenn du nur die Angabe in Grad pro Schritt hast, dann teile 360° durch die Gard pro Schritt Angabe. Zum Beispiel: 360° / 1,8° = 200 PPR. | 

## Links 

Hier sind noch einmal alle verwendeten und erwähnten Bauteile erwähnt:

- [Netzteil 12 V](https://geni.us/XL8mi2)
- [Hohlbuchse 5,5 mm mit Terminal Adapter](https://geni.us/BXMjbb)
- [Nema34 12NM Schrittmotor und Treiber](https://geni.us/hxMw)
## Tipps/Fehlerbehebungen

#### Der Motor dreht sich falsch herum.

Sollte der Motor sich bei der Streckenmessung falsch herum drehen, kann man dies ändern in dem man die Kabel des Motors der Gruppe A oder B anders am Motor-Treiber anschließt.

Also entweder das schwarze und das grüne Kabel tauschen.

| Motor    | Motor-Treiber | 
| -------- | ------------- | 
| Grün     | A+            |
| Schwarz  | A-            | 
| Rot      | B+            |
| Blau     | B-            | 

 Oder das rote und das blaue Kabel tauschen:

| Motor    | Motor-Treiber | 
| -------- | ------------- | 
| Schwarz  | A+            |
| Grün     | A-            | 
| Blau     | B+            |
| Rot      | B-            | 

Vorrausgesetzt der Motor und der Treiber sind korrekt verdrahtet und beschriftet.

Du kannst auch prüfen ob Scharz/Grün bzw. Rot/Blau wirklich in eine Gruppe sind. Wenn du mit dem Multimeter (im Durchgangsmodus mit Piep oder Ohmmessung) auf prüfst, müssen die Kabel, die in einer Gruppe sind eine Verbindung haben (also piepen).
