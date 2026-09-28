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

`watch-upstream.yml` → `sync-upstream.yml` → `generate-dats.yml` → webhook.
Le homelab (CT 105) n'intervient **qu'à la toute fin**, appelé par le
webhook. Ne jamais proposer de déplacer un maillon (détection des commits
amont, build, génération des DAT) vers le homelab : si un mécanisme GitHub
ne convient pas, chercher une autre solution GitHub.

Détection des commits amont : GitHub ne notifie pas un fork, et son
`on: schedule` s'est révélé inutilisable (2 exécutions en 8 h pour un
`*/15`). D'où `watch-upstream.yml`, un job permanent qui sonde toutes les
2 min et se relance avant la limite de 6 h.

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
