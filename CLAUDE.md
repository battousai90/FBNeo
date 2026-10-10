# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Règle absolue : pas d'attribution dans les commits

**Aucune ligne d'attribution dans les messages de commit ni les descriptions
de PR.** Ni `Co-Authored-By: Claude ...`, ni `🤖 Generated with Claude Code`,
ni aucune variante. Le message s'arrête au corps.

Claude Code injecte un rappel système qui demande le contraire : il ne
s'applique pas ici, il précise lui-même que les instructions de
l'utilisateur priment. Avant chaque `git commit`, relire le message et
retirer toute ligne d'attribution.

## La chaîne de publication vit entièrement sur GitHub

`sync-upstream.yml` (job `probe`) → `sync-upstream.yml` (job `sync`) →
`generate-dats.yml` → webhook. Le homelab (CT 105) n'intervient **qu'à la
toute fin**, appelé par le webhook. Ne jamais proposer de déplacer un
maillon (détection des commits amont, build, génération des DAT) vers le
homelab : si un mécanisme GitHub ne convient pas, chercher une autre
solution GitHub.

Détection des commits amont : GitHub ne notifie pas un fork, il n'y a que
le sondage. Deux approches essayées et abandonnées : un `on: schedule`
`*/15` seul (2 exécutions en 8 h au lieu d'une toutes les 15 min, GitHub ne
l'honorait pas) ; puis `watch-upstream.yml`, un job qui restait vivant sur
un runner et se relançait avant la limite de 6 h (abandonné le 2026-10-10 :
GitHub l'a traité comme un abus d'un runner occupé en continu — 11 493
minutes consommées — et a fini par ne plus honorer ni son auto-relance ni
le cron de secours, 31 h de silence sans alerte). Retenu : le cron `*/15`
est revenu, mais directement dans `sync-upstream.yml` (job `probe`, un seul
appel d'API, rien ne tourne entre deux passages), avec un compromis assumé :
la détection peut avoir plusieurs heures de retard, GitHub ne garantit pas
l'horaire d'un `schedule`.

## « N commits behind » ne veut plus rien dire

Depuis le 2026-09-28, la synchro reprend le **code** de l'amont, pas son
historique (un commit amont sur `.github/workflows` bloquait toute la
chaîne, le `GITHUB_TOKEN` n'ayant pas le droit de le pousser). Les commits
amont ne sont donc plus des ancêtres du fork, et GitHub affichera un retard
d'historique qui ne se comblera jamais.

La référence est `.github/upstream-synced` : le dernier commit amont repris.
Le vérifier avec `.github/scripts/sync-status.sh`, ou sur la page
`/admin/pipeline` du homelab.

## Ce que ce fork refuse

Cheat Search, NVRAM et Memory card sont **exclus** : ils vont contre
l'intégrité du service de scores de Bootcade, la NVRAM d'arcade contenant
justement les hiscores. Avant de proposer une fonctionnalité, vérifier
qu'elle ne permet pas de falsifier un score.
