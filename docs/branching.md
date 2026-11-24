# Branch-Strategie

## Ziel

- Eigene Features entstehen im Fork und lassen sich später **sauber** als Pull Request in das
  Hauptprojekt (`upstream`) übernehmen.
- Agent-Dateien (Claude, Notizen) bleiben im Fork, gelangen aber nie in einen PR.

## Überblick

```
upstream/dev ──●──●──●──────────●──●──►            (Maintainer, Ziel der PRs)
                     \                \
feature/immich        ●──●──●  (rebase) ●──●──►    Fork, PR -> upstream/dev
                                        
claude          ◆──◆──◆──►                         Fork, verwaist, nie mergen
```

| Branch | Wo | Basis | Wird gemergt nach |
| :--- | :--- | :--- | :--- |
| `main`, `dev` | upstream | – | – (Fork-Kopien bleiben unberührt) |
| `feature/<thema>`, `fix/<thema>` | Fork | `upstream/dev` | `upstream/dev` per PR |
| `claude` | Fork | keine (verwaist) | nie |

Warum `dev` als Basis: Die Maintainer entwickeln auf `dev` und mergen `dev` → `main`. Externe
PRs (#12, #14) gingen ebenfalls nach `dev`. `dev` ist gerade 11 Commits vor `main`.

## Regeln

1. **Neuer Feature-Branch**
   ```powershell
   git fetch upstream
   git switch -c feature/<thema> upstream/dev
   git push -u origin feature/<thema>
   ```
2. **Aktuell halten**: Solange kein PR offen ist, auf `upstream/dev` rebasen
   (`git fetch upstream; git rebase upstream/dev; git push --force-with-lease`). Sobald ein PR
   offen ist und Review läuft, stattdessen `upstream/dev` hineinmergen, damit Review-Kommentare
   nicht verloren gehen.
3. **Nur Projektdateien** in Feature-Branches. Keine `AGENTS.md`, `CLAUDE.md`, `.claude/`. Das
   sichern `.git/info/exclude` und der `pre-push`-Hook aus `setup.ps1` ab.
4. **Kein Push auf `main`/`dev` im Fork.** `deploy.yml` reagiert auf Pushes auf diese Branches;
   im Fork würde der HIL-Job ewig auf den self-hosted Runner warten. Feature-Branches und
   `claude` lösen keinen Workflow aus.
5. **Nie nach `upstream` pushen.** Der Hook blockiert das zusätzlich.
6. **PR**: von `0llirocks:feature/<thema>` nach `paperlesspaper:dev`, erst wenn der Nutzer es
   ausdrücklich will. Commits im Stil `feat(main) …` / `feat(web) …`.
7. **Eigene Features, die upstream nicht will**, bleiben als Feature-Branch im Fork. Für ein
   eigenes Gerät kann ein lokaler Integrations-Branch (`local/integration`) mehrere
   Feature-Branches zusammenführen; er wird nie gepusht und nie Basis eines PRs.

## Agent-Dateien

- Leben im verwaisten Branch `claude`, ausgecheckt als Worktree im Unterordner `.claude-local/`.
- Im Repo-Wurzelordner liegen nur nicht versionierte Verweise `CLAUDE.md` / `AGENTS.md`.
- Ausschlüsse in `.git/info/exclude` (lokal, wird nie gepusht):
  `.claude-local/`, `/CLAUDE.md`, `/AGENTS.md`, `/CLAUDE.local.md`, `.claude/`.
- Da die Dateien nicht versioniert sind, überleben sie jeden Branch-Wechsel.
- Änderungen daran: im Ordner `.claude-local/` committen, `git push origin claude`.
