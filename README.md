# Branch `claude`: lokale Agent-Dateien (nie mergen)

Dieser Branch enthält **keinen Projektcode**. Er ist ein verwaister Branch (keine gemeinsame
Historie mit `main`/`dev`) und hält nur die Dateien für KI-Agenten (Claude Code u. a.) und
Arbeitsnotizen. Er wird **nie** in einen anderen Branch gemergt und **nie** an `upstream`
(paperlesspaper/paperlesspaper-firmware-offline) gepusht.

| Datei | Zweck |
| :--- | :--- |
| `AGENTS.md` | Regeln für Agenten in diesem Fork (wird von `CLAUDE.md` importiert) |
| `CLAUDE.md` | Einstieg für Claude Code |
| `docs/branching.md` | Branch-Strategie: Fork, Upstream, Feature-Branches |
| `docs/immich.md` | Konzept und Stand der Immich-Integration |
| `setup.ps1` | Richtet einen lokalen Klon ein (Worktree, Ausschlüsse, Schutz-Hook) |

## Einrichtung in einem lokalen Klon

```powershell
git clone https://github.com/0llirocks/paperlesspaper-firmware-offline.git
cd paperlesspaper-firmware-offline
git fetch origin claude
git worktree add .claude-local claude
powershell -ExecutionPolicy Bypass -File .claude-local\setup.ps1
```

Danach liegt dieser Branch im Unterordner `.claude-local/`. Im Repo-Wurzelordner entstehen zwei
kleine, **nicht versionierte** Verweisdateien `CLAUDE.md` und `AGENTS.md`, die auf
`.claude-local/` zeigen. Alles davon steht in `.git/info/exclude` und taucht in keinem
Feature-Branch auf. Änderungen an den Agent-Dateien werden im Ordner `.claude-local/`
committet und mit `git push origin claude` gesichert.
