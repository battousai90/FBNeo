#!/bin/bash
# Repond a "suis-je a jour sur l'amont ?" sans rien installer ni s'authentifier.
#
# Le bandeau "N commits behind" de la page GitHub du fork ne repond plus a
# cette question : depuis 2026-09-28, sync-upstream.yml reprend le CODE de
# finalburnneo/FBNeo, pas son historique (un commit amont sur .github/workflows
# bloquait toute la chaine, le GITHUB_TOKEN n'ayant pas le droit de le pousser).
# Les commits amont ne sont donc plus des ancetres du fork, et GitHub les
# compte indefiniment comme "en retard" alors que leur code est bien la.
#
# La reference est .github/upstream-synced : le dernier commit amont repris.
set -euo pipefail

api() { curl -fsS --max-time 20 -H 'Accept: application/vnd.github+json' "https://api.github.com/$1"; }
sha() { api "repos/$1/commits/master" | python3 -c 'import json,sys; print(json.load(sys.stdin)["sha"])'; }

up="$(sha finalburnneo/FBNeo)"
synced="$(api 'repos/battousai90/FBNeo/contents/.github/upstream-synced?ref=master' \
    | python3 -c 'import base64,json,sys; print(base64.b64decode(json.load(sys.stdin)["content"]).decode().strip())')"

echo "amont   : ${up:0:9}"
echo "repris  : ${synced:0:9}"
if [ "$up" = "$synced" ]; then
    echo "-> a jour."
else
    echo "-> en retard. Commits pas encore repris :"
    api "repos/finalburnneo/FBNeo/compare/$synced...master" \
        | python3 -c 'import json,sys; [print("   ", c["sha"][:9], c["commit"]["message"].splitlines()[0]) for c in json.load(sys.stdin)["commits"]]'
    echo "   (le watcher les prend dans les 2 min ; sinon Actions -> Watch upstream -> Run workflow)"
fi
