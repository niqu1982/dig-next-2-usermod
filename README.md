================================= EN ===============================



Base on quinled https://quinled.info/dig-next-2/


QuinLED Dig-Next-2 WLED Usermod Repository

WLED usermod for QuinLED Dig-Next-2 relay control – extension for WLED v0.16.x enabling independent control of two LED lighting channels and master relay.
Key Features:
text

Bus 0: GPIO2, LED 1

Bus 1: GPIO4, LED 2

    Channel 1 (CH1): GPIO20+22 (5A1+5A2), controlled by button GPIO34

    Channel 2 (CH2): GPIO21 (10A1), controlled by button GPIO35

    Master Relay: GPIO5 – turns ON when either channel is active

    Hardware debounced buttons (pulled-high)

    LED segment synchronization via onStateChange()

Critical Fix:

Resolves master relay shutdown issue during channel switching – new stateful logic with guard time prevents state override by transient WLED segment changes.



Installation:

    Copy usermoddignext2relay folder to usermods/

    platformiooverride.ini: customusermods=usermoddignext2relay

    IMPORTANT: Do NOT configure GPIO 5,20,21,22 as standard WLED relays

    Set GPIO 34,35 as None in Button section



# dig-next-2-usermod
user mod relays and buttons for dig-next-2 relay
QuinLED Dig-Next-2 WLED Usermod Repository


=================================== PL   =====================================
Na podstawie quinled https://quinled.info/dig-next-2/

WLED usermod sterujący przekaźnikami QuinLED Dig-Next-2 – rozszerzenie dla WLED v0.16.x do niezależnego sterowania dwoma kanałami oświetlenia LED i master relay.
Główne funkcjonalności:

    Kanał 1 (CH1): GPIO20+22 (5A1+5A2), sterowany przyciskiem GPIO34

    Kanał 2 (CH2): GPIO21 (10A1), sterowany przyciskiem GPIO35

    Master Relay: GPIO5 – włączany gdy którykolwiek kanał aktywny

    Hardware debounce przycisków (pulled-high)

    Synchronizacja z segmentami LED przez onStateChange()

Kluczowa poprawka:

Rozwiązuje problem wyłączania master relay przy przełączaniu kanałów – nowa logika stateful z guard time zapobiega nadpisywaniu stanów przez chwilowe zmiany segmentów WLED.
Konfiguracja sprzętowa:


Bus 0: GPIO2, LED 1


Bus 1: GPIO4, LED 2

Instalacja:

    Skopiuj folder usermoddignext2relay do usermods/

    platformiooverride.ini: customusermods=usermoddignext2relay

    WAŻNE: GPIO 5,20,21,22 NIE konfiguruj jako standardowe relay WLED

    GPIO 34,35 ustaw jako None w sekcji Button

Repozytorium zawiera debug logi UART i wersje z różnymi strategiami synchronizacji relay z segmentami LED.


