# Steinel NightmatIQ Plus

Standardowy firmware AR01V3 zawiera opcjonalną integrację z wcześniej
skonfigurowaną siecią Bluetooth Mesh Steinel. Nie istnieje osobny obraz
firmware: te same pliki `ar01v3-01.yaml`–`ar01v3-10.yaml` zachowują RF 433,92
MHz, IR, ESP-NOW, import Flippera, OTA, stronę WWW i API Home Assistant.

## Dwa tryby Bluetooth

AR01V3 uruchamia tylko jeden z dwóch trybów:

- **Bluetooth Proxy** — tryb domyślny, także gdy NightmatIQ nie jest
  skonfigurowany albo został wyłączony;
- **Bluetooth Mesh** — aktywny po włączeniu integracji NightmatIQ.

Zmiana trybu powoduje automatyczny restart. Wyłączenie integracji na stronie WWW
nie usuwa kluczy ani adresów Mesh. Ponowne włączenie nie wymaga kolejnego
logowania do chmury ani pobierania kopii. Oddzielny przycisk **Remove
configuration** służy do trwałego usunięcia zapisanych danych.

## Wymagania

- NightmatIQ Plus widoczny w aplikacji Steinel Connect i zsynchronizowany z
  kontem Steinel;
- AR01V3 umieszczony w zasięgu Bluetooth urządzenia;
- zaufana sieć lokalna podczas pierwszej konfiguracji;
- standardowy `esphome/secrets.yaml`; hasła Steinel i kluczy Mesh nie dopisuje
  się do tego pliku.

## Wgranie i pierwsza konfiguracja

1. Zbuduj i wgraj zwykły plik wybranego odbiornika, na przykład
   `esphome/ar01v3-03.yaml`.
2. Otwórz chronioną stronę AR01V3, a następnie w bloku **Steinel NightmatIQ
   Plus** naciśnij **Configuration Page** albo przejdź bezpośrednio do
   `http://ADRES_URZADZENIA/steinel`.
3. Wpisz e-mail i hasło konta Steinel, a następnie wybierz **Download network
   list**.
4. Wybierz sieć zawierającą NightmatIQ. Puste stare rekordy są pomijane.
5. Adres węzła pozostaw pusty, chyba że sieć zawiera kilka podobnych urządzeń.
6. **IV Index** pozostaw na `0`, aby Mesh odzyskał bieżącą wartość z
   uwierzytelnionego komunikatu sieci. AR01V3 wykorzysta też ostatnią
   uwierzytelnioną wartość zapamiętaną dla wybranej sieci Steinel. Ręczne
   podanie wartości może przyspieszyć pierwszą synchronizację, gdy pamięć
   podręczna jest pusta.
7. Wybierz **Install on AR01V3**. Bramka pobierze backup przez HTTPS, sprawdzi
   go, zapisze tylko potrzebne dane i uruchomi się ponownie w trybie Mesh.

Duży backup sieci nie jest przechowywany w RAM. Podczas importu AR01V3 zapisuje
go strumieniowo w nieaktywnym slocie OTA i odczytuje tylko pola potrzebne do
integracji. Nie zmienia to tablicy partycji, działającego obrazu ani danych NVS;
następna aktualizacja OTA normalnie nadpisze ten roboczy slot. Rozwijana sekcja
**Diagnostics** na dole strony pokazuje rozmiar ostatniej odpowiedzi oraz stan
pamięci przed operacją chmurową i po zwolnieniu Bluetooth.

Hasło Steinel nie jest zapisywane we flash ani zwracane przez lokalne API.
Przeglądarka wysyła je jednak do AR01V3 przez lokalny HTTP, więc pierwszą
konfigurację wykonuj wyłącznie w zaufanym LAN. Po udanym imporcie codzienna
komunikacja z NightmatIQ jest lokalna i nie wymaga chmury Steinel.

## Wyłączenie bez usuwania danych

Na `/steinel` wybierz **Disable NightmatIQ**. AR01V3 zapisze stan wyłączenia i
po restarcie wróci do Bluetooth Proxy. Sieć, klucze, adresy i IV Index pozostaną
w pamięci. **Enable NightmatIQ** przywraca tryb Mesh z tej samej konfiguracji.

**Remove configuration** usuwa zapisane dane Mesh i służy tylko do zmiany sieci
lub całkowitego wycofania konfiguracji. Czyszczenie odbywa się na miejscu w obu
trybach Bluetooth. Gdy strona potwierdzi zakończenie, można od razu zaimportować
nową sieć — ręczny restart bramki nie jest potrzebny.

## Home Assistant

Integracja ESPHome automatycznie tworzy osobne urządzenie `Steinel NightmatIQ
Plus` i przypisuje do niego encje:

- `NightmatIQ Illuminance` — natężenie światła;
- `NightmatIQ Twilight Threshold` — próg zmierzchowy 1–1500 lx;
- `NightmatIQ Mode` — `Auto`, `Always On` albo `Always Off`;
- `NightmatIQ Actual Light Output` — rzeczywisty stan wyjścia odczytany z
  uwierzytelnionej odpowiedzi Mesh; pojedyncze braki odpowiedzi nie zmieniają
  ostatniego potwierdzonego ON/OFF, a encja staje się niedostępna dopiero po
  pięciu minutach bez prawidłowej odpowiedzi albo natychmiast po wyłączeniu lub
  usunięciu integracji;
- `NightmatIQ Mesh Ready` i `NightmatIQ Status` — diagnostyka;
- `NightmatIQ Installed Firmware`, `NightmatIQ Hardware Revision`, `NightmatIQ
  Manufacturer`, `NightmatIQ Company ID` i `NightmatIQ Product ID` — dane
  diagnostyczne odczytane z urządzenia. Identyfikatory szesnastkowe używają
  małych liter; zarejestrowany Company ID `0x0563` jest wyświetlany jako
  `0x0563 (Steinel GmbH)`;
- `NightmatIQ Refresh` — wymusza natychmiastowe ponowne odczytanie danych z
  urządzenia; nie zmienia jego konfiguracji.

Opcjonalny przykład karty znajduje się w
`home-assistant/nightmatiq_dashboard_card.yaml`. Po dodaniu urządzenia zastąp w
nim przykładowe identyfikatory rzeczywistymi encjami.

## Zgodność

Integrację przetestowano ze Steinel IS Digi NM 2E6915 NightmatIQ Plus.
Standardowa komunikacja Bluetooth Mesh nie udostępnia wersji działającego
bootloadera, dlatego AR01V3 jej nie wyświetla.
