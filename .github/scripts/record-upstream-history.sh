#!/bin/bash
# Rattache l'historique amont au fork, sans toucher au code : un merge
# `-s ours` sur le dernier commit amont repris (.github/upstream-synced).
# GitHub recompte alors « N commits behind » depuis ce point.
#
# A lancer depuis le poste, avec les identifiants git de l'utilisateur : le
# GITHUB_TOKEN d'un workflow ne peut pas pousser les commits amont qui touchent
# .github/workflows, et ce fork n'a volontairement aucun secret GitHub.
set -euo pipefail
cd "$(git rev-parse --show-toplevel)"
git remote get-url upstream >/dev/null 2>&1 || git remote add upstream https://github.com/finalburnneo/FBNeo.git
git fetch -q upstream
git fetch -q origin
git switch -q master
git merge --ff-only -q origin/master
S="$(tr -d '[:space:]' < .github/upstream-synced)"
if git merge-base --is-ancestor "$S" HEAD 2>/dev/null; then
  echo "Deja rattache jusqu'a ${S:0:9}."; exit 0
fi
git merge -s ours --allow-unrelated-histories -m "Record upstream history up to ${S:0:9} so GitHub counts commits behind again

The tree is unchanged: the code of ${S:0:9} was already synced." "$S"
git push origin master
git rev-list --left-right --count HEAD...upstream/master | awk '{print $2 " commit(s) amont pas encore repris"}'
