# Richtet einen lokalen Klon des Forks für die Arbeit mit Agenten ein.
# Aufruf im Repo-Wurzelordner, nachdem der Branch `claude` als Worktree in .claude-local liegt:
#   git worktree add .claude-local claude
#   powershell -ExecutionPolicy Bypass -File .claude-local\setup.ps1
$ErrorActionPreference = 'Stop'

$root = (git rev-parse --show-toplevel).Trim()
$gitDir = (git rev-parse --git-common-dir).Trim()
if (-not [System.IO.Path]::IsPathRooted($gitDir)) { $gitDir = Join-Path $root $gitDir }
Set-Location $root

# 1. Remotes
$remotes = git remote
if ($remotes -notcontains 'upstream') {
    git remote add upstream https://github.com/paperlesspaper/paperlesspaper-firmware-offline.git
}
git remote set-url --push upstream DISABLED # ein versehentlicher Push nach upstream schlägt fehl
git fetch upstream

# 2. Agent-Dateien aus allen Branches heraushalten (nur lokal, wird nie gepusht)
$utf8 = New-Object System.Text.UTF8Encoding($false) # ohne BOM
function Write-PlainFile([string]$path, [string]$text) {
    [System.IO.File]::WriteAllText($path, ($text -replace "`r`n", "`n"), $utf8)
}

$exclude = Join-Path $gitDir 'info\exclude'
New-Item -ItemType Directory -Force (Split-Path $exclude) | Out-Null
$current = @()
if (Test-Path $exclude) { $current = @(Get-Content $exclude) }
$lines = @('.claude-local/', '/CLAUDE.md', '/AGENTS.md', '/CLAUDE.local.md', '.claude/')
$missing = @($lines | Where-Object { $current -notcontains $_ })
if ($missing.Count -gt 0) { Write-PlainFile $exclude ((@($current) + $missing) -join "`n") }

# 3. Verweise im Wurzelordner
Write-PlainFile (Join-Path $root 'CLAUDE.md') "@.claude-local/CLAUDE.md`n"
Write-PlainFile (Join-Path $root 'AGENTS.md') "Agent-Regeln: siehe .claude-local/AGENTS.md (Branch ``claude``, nicht Teil des Projekts).`n"

# 4. pre-push-Hook: kein Push nach upstream, nicht nach main/dev im Fork, keine Agent-Dateien
#    in Projekt-Branches
$hook = Join-Path $gitDir 'hooks\pre-push'
@'
#!/bin/sh
# Installed by .claude-local/setup.ps1
remote="$1"
case "$remote" in
  upstream) echo "pre-push: never push to upstream" >&2; exit 1 ;;
esac
z=0000000000000000000000000000000000000000
while read local_ref local_sha remote_ref remote_sha; do
  case "$remote_ref" in
    refs/heads/main|refs/heads/dev)
      echo "pre-push: do not push $remote_ref to the fork (starts deploy.yml)" >&2; exit 1 ;;
    refs/heads/claude) continue ;;
  esac
  [ "$local_sha" = "$z" ] && continue
  if git ls-tree -r --name-only "$local_sha" | grep -qE '^(AGENTS\.md|CLAUDE\.md|CLAUDE\.local\.md|\.claude/|\.claude-local/)'; then
    echo "pre-push: $remote_ref contains agent files (AGENTS.md, CLAUDE.md, .claude/)" >&2; exit 1
  fi
done
exit 0
'@ | ForEach-Object { Write-PlainFile $hook $_ } # Git for Windows führt den Hook mit sh aus, LF genügt

Write-Host 'Fertig: upstream eingerichtet (Push gesperrt), Ausschlüsse gesetzt, Verweise und pre-push-Hook angelegt.'
