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
| `immichUrl` | ≤ 128 | 820 (130 B) | `10000010` | read, write |
| `immichApiKey` | ≤ 128 | 950 (130 B) | `10000011` | **write** (wie HTTP-Passwort) |
| `immichAlbumIds` | ≤ 10 UUIDs, Komma-getrennt, ≤ 369 | 1080 (370 B, bis 1950) | `10000012` | read, write |

(`1000000e`/`1000000f` belegt auf `dev` die Galerie.)

`imageMode` (`10000002`) akzeptiert zusätzlich `2`. Remote-JSON: `immichUrl`, `immichApiKey`,
`immichAlbumIds` (Array oder Komma-String), `imageMode` (`1` oder `2`).

## API-Key in Immich

Benötigte Berechtigungen: `asset.read` (Zufallssuche), `asset.view` (Vorschau),
`asset.statistics` (nur bei mehreren Alben).

Voraussetzung auf dem Server: Vorschau-Format **JPEG**, **nicht progressiv** (Administration →
Einstellungen → Bild). WebP kann die Firmware nicht lesen; progressive JPEGs nur in 1/8
Auflösung.

## Web-UI

- Ansicht „Bild“ → „Abruf per URL“: Schalter *Eigene URL* / *Immich*.
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

Branch `feature/immich` (Basis `upstream/dev` e8ba2ee), Commit 89013f0, 2026-11-24:

- [x] Firmware (`src/immich.*`, `src/main.cpp`, `src/types.h`)
- [x] Web-UI (`DeviceBleInterface.js`, `app.js`, `index.html`, `styles.css`)
- [x] Simulator, Konsistenz-Test, Web-Test `test_save_immich_settings`, HIL-Settings, README
- [x] Unit-Tests geschrieben (`test/test_immich`, `[env:native]`)
- [ ] **Nichts davon kompiliert oder ausgeführt** – Werkzeuge fehlen noch:
  `pio run -e epd7`, `pio run -e epd13`, `pio test -e native`,
  `cd web; npm ci; npm run build`, `python -m pytest webtest`
- [ ] Test gegen das echte Immich des Nutzers
- [ ] Hardware (eigenes Display / HIL upstream)

Nachtrag Commit 2fee94b (Orientierung):
- Immich kann nicht nach Format suchen (`SearchFilter` hat kein width/height). Das Gerät holt 10
  Zufallsfotos und nimmt das erste, dessen Seitenverhältnis höchstens 1,5-fach vom Display
  abweicht (7,3" quer, 13,3" hoch), sonst das nächstliegende. `width`/`height` sind bei Immich
  schon EXIF-gedreht (`metadata.service.ts`, `isSidewards`).
- Immich ≥ 3.2: eine Suche mit `filter.albumIds.any` (ODER) statt Zählen pro Album; alte Felder
  dürfen nicht mit `filter` gemischt werden. Bei HTTP 400 Rückfall auf die alte Suche
  (`immichSearchShape` im RTC-Speicher).
- Die Firmware dreht Bilder nie um 90° (Auto-Rotation nur 180°). „Passend“ heißt deshalb
  passend zum Display-Raster, nicht zur Seite, auf der der Rahmen steht. Hochkant-Betrieb
  des 7,3" bräuchte eine 90°-Drehung in der Bildaufbereitung/Anzeige: eigenes Feature,
  nur mit Hardware testbar.

Offene Punkte:
- `[env:native]` erbt aus `[env]`; ob die leeren Overrides (`framework =`, `board =`) in
  PlatformIO greifen, beim ersten `pio test -e native` prüfen.
- Bildzählung bei mehreren Alben kostet pro Aufwachen einen Request je Album; ggf. später
  in RTC-Speicher cachen.
