# Agent-Regeln für den Fork 0llirocks/paperlesspaper-firmware-offline

Diese Datei liegt im Branch `claude` (Ordner `.claude-local/`) und gehört nicht zum Projekt.

## Projektregeln

Die Regeln des Projekts selbst stehen in `../copilot-instructions.md` (Firmware, Web-UI,
Prettier, Design). Sie gelten unverändert:

@../copilot-instructions.md

## Repositories

- `origin` = Fork `0llirocks/paperlesspaper-firmware-offline` (hierhin pushen)
- `upstream` = `paperlesspaper/paperlesspaper-firmware-offline` (nur lesen, **nie pushen**)
- Auf GitHub **nichts** im Upstream-Repo kommentieren, keine Issues/PRs dort anlegen oder
  ändern, außer der Nutzer verlangt es ausdrücklich für genau diesen Schritt.

## Branches (Details: `docs/branching.md`)

- Feature-Branches zweigen von `upstream/dev` ab: `feature/<thema>` bzw. `fix/<thema>`.
- `main` und `dev` im Fork nie direkt bearbeiten und **nie in den Fork pushen**: ein Push auf
  `main`/`dev` startet dort `deploy.yml`, dessen HIL-Job auf einen Runner wartet, den es im Fork
  nicht gibt.
- Agent-Dateien (`AGENTS.md`, `CLAUDE.md`, `.claude/`, Notizen) nur im Branch `claude`, nie in
  einem Feature-Branch.
- Commit-Stil wie upstream: `feat(main) …`, `fix(web) …`, `feat(deploy) …` (Bereich in
  Klammern: `main` = Firmware, `web` = Web-UI, `deploy` = CI/Tests).

## Dinge, die gemeinsam geändert werden müssen

Limits und BLE-UUIDs stehen an mehreren Stellen und werden von
`webtest/test_consistency.py` verglichen:

- Firmware: `src/main.cpp`, `src/types.h`
- Web-UI: `web/src/app.js` (`DEVICE_LIMITS`), `web/src/DeviceBleInterface.js`, `web/src/index.html`
- Simuliertes Gerät: `webtest/simdevice.py`
- Hardware-Tests: `hil/settings.py`
- Doku: `README.md`, `settings_sample.json`

## EEPROM-Belegung (2048 Bytes)

| Adresse | Inhalt |
| :--- | :--- |
| 0–39 / 40–105 | WLAN-SSID / -Passwort |
| 210, 220, 230 | Sleep-Timeout, Orientierung, Anzeigeinhalt |
| 499 | Init-Marker (42) |
| 500 + 5 … 500 + 819 | Settings (`saveSettingsToFlash`) |
| 500 + 820 … | Erweiterungen (Immich, siehe `docs/immich.md`) |

## Werkzeuge

Lokal sind Python, PlatformIO, Node.js und ein Host-Compiler noch nicht installiert. Build
(`pio run -e epd7`) und Tests (`python -m pytest webtest`) erst ausführen, wenn sie vorhanden
sind, und im Bericht klar sagen, was ungetestet ist.
