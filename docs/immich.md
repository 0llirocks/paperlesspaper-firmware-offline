# Immich-Integration (Konzept und Stand)

Bezug: Upstream-Issue #13 „Immich Support“. Vorarbeit: Branch `immich` von Adrian-Grimm
(Favoriten statt Alben, auf altem Stand, UUID `1000000d` kollidiert inzwischen mit „Firmware
version“). Diese Umsetzung baut neu auf `upstream/dev` auf.

## Verhalten

Neuer Bildmodus `imageMode = 2` (Immich). Bei jedem Aufwachen:

1. WLAN verbinden, ggf. Remote-Settings-JSON laden (wie bisher).
2. **Album auslosen** (nur bei mehr als einem Album): für jedes Album
   `POST /api/search/statistics` mit `{"albumIds":["<id>"],"type":"IMAGE"}` → `total`.
   Ein Album wird mit Wahrscheinlichkeit `total / Summe` gewählt, so erscheint **jedes Foto
   gleich oft**. Grund: Immich verknüpft mehrere `albumIds` mit UND (nur Fotos, die in allen
   Alben liegen, siehe `inAlbums()` in `server/src/utils/database.ts`), ein einzelner Aufruf mit
   allen Alben ginge also nicht.
3. **Zufallsfoto**: `POST /api/search/random` mit
   `{"albumIds":["<id>"],"type":"IMAGE","size":2,"withExif":false,"withPeople":false}`. Das erste
   Foto, das nicht das zuletzt gezeigte ist (`RTC_DATA_ATTR`, übersteht Deep Sleep), wird
   genommen.
4. **Vorschau laden**: `GET /api/assets/<id>/thumbnail?size=preview` → JPEG (Standard ca.
   1440 px lange Kante) in den SPI-Flash, dann wie jedes JPEG auf dem Gerät skaliert und
   gedithert (`processImageFile`).
5. ETag/304-Logik entfällt (jedes Bild ist absichtlich neu). Fehler → vorhandene Fehleranzeige
   und Retry-Backoff.

Header für alle Aufrufe: `x-api-key: <key>`. Die `X-OpenPaper-*`-Telemetrie wird an Immich
**nicht** gesendet.

## Einstellungen

| Name | Länge | EEPROM (`500 +`) | BLE-UUID (`…-0000-0000-0000-000000000001`) | Eigenschaft |
| :--- | :--- | :--- | :--- | :--- |
| `immichUrl` | ≤ 128 | 820 (130 B) | `1000000e` | read, write |
| `immichApiKey` | ≤ 128 | 950 (130 B) | `1000000f` | **write** (wie HTTP-Passwort) |
| `immichAlbumIds` | ≤ 10 UUIDs, Komma-getrennt, ≤ 369 | 1080 (372 B, bis 1951) | `10000010` | read, write |

`imageMode` (`10000002`) akzeptiert zusätzlich `2`. Remote-JSON: `immichUrl`, `immichApiKey`,
`immichAlbumIds` (Array oder Komma-String), `imageMode` (`1` oder `2`).

## API-Key in Immich

Benötigte Berechtigungen: `asset.read` (Zufallssuche), `asset.view` (Vorschau),
`asset.statistics` (nur bei mehreren Alben).

Voraussetzung auf dem Server: Vorschau-Format **JPEG**, **nicht progressiv** (Administration →
Einstellungen → Bild). WebP kann die Firmware nicht lesen; progressive JPEGs nur in 1/8
Auflösung.

## Web-UI

- Tab „Gerät“: Auswahl der Bildquelle *Download-URL* / *Immich*.
- Felder: Immich-URL, API-Key (Passwortfeld, „leer = gespeicherten behalten“), Alben (Liste).
- Album-Link aus Immich einfügen (`https://immich.example/albums/<uuid>`) → URL und ID werden
  übernommen. Kein Abruf der Albumliste aus dem Browser: CORS/Mixed Content, und der Key darf
  nie über die öffentlichen CORS-Proxys (`fetchWithProxy`) laufen.

## Tests

1. Host-Unit-Tests (PlatformIO `native`) für das reine C++-Modul (`src/immich.h`): JSON bauen
   und lesen, Album gewichten, Album-Liste prüfen.
2. Gegen das echte Immich des Nutzers: Skript mit denselben Aufrufen wie die Firmware.
3. `webtest/` mit `simdevice.py`, `test_consistency.py`.
4. Hardware: HIL-Pipeline upstream bzw. eigenes Display.

## Stand

- [ ] Firmware
- [ ] Web-UI
- [ ] Simulator, Konsistenz-Test, HIL-Settings, Doku
- [ ] Build (`pio run -e epd7`, `-e epd13`) – Werkzeuge noch nicht installiert
- [ ] Tests
