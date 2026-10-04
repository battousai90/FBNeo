# Plan d'intégration — Fork FBNeo + fbneo-launcher (Bootcade)

Établi le 2026-09-15. Estimations issues de la lecture du code et des makefiles,
**rien n'a été compilé**.

## Règle de répartition

Le **Fork** implémente le mécanisme et l'expose en CLI ou dans `fbneo.ini`.
Le **Launcher** pilote via son `SettingsPanel`. Aucun GUI n'est écrit dans
l'émulateur. C'est déjà le contrat de `-dat`.

Raison technique : le Launcher lance FBNeo via `fork()`+`execvp`
(`MainWindow.cpp:95`, `spawn_process`). C'est un **processus séparé** — il n'a
accès ni à la RAM émulée, ni au framebuffer, ni au buffer audio, ni à la boucle
image par image. Tout ce qui touche à l'émulation doit donc vivre dans le Fork.

---

## Phase 0 — Mise en jambe · ~1 semaine · Fork

Aucune dépendance. Valide le circuit build/test/release avant les gros morceaux.

| Tâche | Détail | Effort |
|---|---|---|
| Save State UNDO | `state.o` déjà linké en SDL2. `state.cpp:376` `BurnStateUNDO()` n'est appelé que par win32. Entrée menu + raccourci. | ~1 h |
| Dump WAV | Déplacer `win32/wave.cpp` (119 l., 0 dépendance Windows) vers `src/burner/`, ajouter aux 2 makefiles. | ~2 h |
| `SDL_LockTexture` | `vid_sdl2.cpp:370` utilise `SDL_UpdateTexture` sur une texture STREAMING — une copie de trop. | ~2 h |
| Filtres softfx CPU | Régression SDL1→SDL2 : `makefile.sdl:68` a les 11 scalers (xBR, CRT, 2xSaI, EPX, DDT3x, 2xPM, hq2xS/3xS), `makefile.sdl2` non. Hooks en place, appel commenté à `vid_sdl2opengl.cpp:407`. Ajouter `nVidBlitterOpt` à `sdl/config.cpp:109`. | ~1-2 j |

## Phase 1 — Intégrité des scores · ~1 semaine · Fork

**À faire tôt : seul poste qui protège quelque chose déjà en production.**

`hiscore.cpp:154` `CheckHiscoreAllowed()` ne teste que `EnableHiscores` et
`BDF_HISCORE_SUPPORTED` — **rien sur les cheats**. Or SDL a l'activation de
cheats (`sdl2_gui_ingame.cpp:1591`) et `EnableHiscores = 1` par défaut
(`sdl/main.cpp:338`). N'importe qui peut tricher puis soumettre à Bootcade.

Modèle existant côté Windows, `win32/run.cpp:627` :
```c
bCheatsAllowed = (nKailleraCheatEnableHack == 0) ? false : true;
```

| Tâche | Effort |
|---|---|
| Drapeau « session souillée » armé dès qu'un cheat est activé, exposé en fin de partie. Le `HiscoreClient` du Launcher refuse la soumission. | ~1 j |

## Phase 2 — Rendu moderne · ~2 semaines · Fork

2.2 dépend de 2.1.

`SurfToTex()` de `vid_sdl2opengl.cpp`, à chaque frame : memcpy ligne par ligne →
`glTexImage2D` (réalloue toute la texture) → `glBegin(GL_QUADS)` (mode immédiat,
déprécié depuis GL 3.0).

| # | Tâche | Effort |
|---|---|---|
| 2.1 | GL moderne : `glTexSubImage2D`, VBO+VAO, PBO pour l'upload | ~2-3 j |
| 2.2 | Pipeline GLSL : shaders CRT, scanlines, masque d'ombre, courbure, grille LCD — sur GPU | ~1 sem. |

## Phase 3 — Support CD · ~1 semaine · Fork **et** Launcher

L'émulation marche déjà : `fbneo neocdz -cd jeu.cue` (`sdl/main.cpp:347`).
`cdlist.cpp` est compilé en SDL2, lit le secteur, extrait le Game ID et résout le
titre via `GetNGCDGameTitle()` (`cdlist.cpp:98`). FBNeo embarque les listes :
**181 jeux NeoCD**, **463 jeux PCE CD**.

| # | Où | Tâche | Effort |
|---|---|---|---|
| 3.1 | Fork | Flag `-cdinfo <image>` : identifie le disque, sort titre/ID/système en JSON. Même contrat que `-dat`. | ~1-2 j |
| 3.2 | Launcher | Scanner `.cue`/`.ccd`/`.chd`, appeler `-cdinfo`, lancer `neocdz -cd`. | ~2-3 j |

**Attention 3.2** : le schéma `games.db` est centré ROM (table `roms` avec
`name`/`size`/`crc`, contrainte `UNIQUE(name, system)`). Une image CD n'est pas
un jeu de ROMs avec CRC — il faut un type de contenu distinct dans le modèle de
données, pas juste un flag.

## Phase 4 — Métadonnées · ~3 jours · Launcher

Ne **pas** porter les 3 993 lignes de `win32/gameinfo.cpp` + `systeminfo.cpp` +
`sel.cpp` : c'est du GUI de navigation, le Launcher a déjà `DatabaseManager`,
`GameRow`, `ThumbnailDownloader`. Ajouter `GetHistoryDatHardwareToken` à la
sortie de `.github/scripts/fbneo-metadata.py`.

## Phase 5 — Gros morceaux · Fork

| Tâche | Détail | Effort |
|---|---|---|
| Lua | Sources dans `src/dep/libs/lua/` (31 `.c`), `luaengine.cpp`+`luasav.cpp` partagés, compilés par aucun makefile SDL. Remplacer `win32/luaconsole.cpp` par un flag `-lua script.lua`. | ~3-5 j |
| Netplay GGPO | Voir ci-dessous | ~2-4 sem. |

### Netplay

La glue est **déjà portable** : `win32/fba_kaillera.cpp`, 257 lignes, zéro
`windows.h`/`HWND`/`WINAPI` — c'est de la sérialisation d'entrées. Tout le
verrou tient dans `src/dep/kaillera/client/net.cpp` : **146 lignes**, un
`LoadLibrary` et 8 pointeurs de fonction.

| # | Tâche | Effort |
|---|---|---|
| 5.1 | Étude de https://github.com/fightcadeorg/fightcade-fbneo — comment GGPO est branché, si leur `makefile.sdl` l'embarque, **et lire `LICENSE.md`** (Fightcade est freeware non commercial) | ~2-3 j |
| 5.2 | Intégration GGPO (MIT depuis 2019) selon les conclusions du 5.1 | ~2-4 sem. |

**GGPO plutôt que Kaillera** : Kaillera est delay-based (on attend le plus lent),
GGPO est rollback. Sur Neo Geo et CPS2 la différence est majeure.
Repli Kaillera si besoin : Open Kaillera (https://sourceforge.net/projects/okai/,
C++ multiplateforme, remplace la DLL) + EmuLinker
(https://sourceforge.net/projects/emulinker/, serveur à héberger sur le homelab).
Vitalité d'okai non vérifiée.

---

## Écarté, et pourquoi

| | Raison |
|---|---|
| Cheat Search, NVRAM, Memory card | Contre l'intégrité du système de scores — la NVRAM d'arcade *contient* les hiscores |
| Enregistrement AVI | `win32/avi.cpp` est bâti sur `vfw.h`. `ffmpeg -f x11grab` piloté par le Launcher évite 1-2 semaines de réécriture |
| Palette viewer | `winuser.h` + dialogues Win32, outil de debug, valeur faible |
| Debugger | 1 592 lignes de GUI Win32, public nul sur la cible |
| Kaillera par DLL | Remplacé par GGPO |
| Localisation | Le Launcher a déjà `i18n.cpp` et `locale/` |
| **Systèmes émulés** | **Aucun écart** : `makefile.mingw` et `makefile.sdl2` incluent tous deux `makefile.burn_rules` → les mêmes 660 pilotes. Le Neo Geo CD marche déjà en SDL. |

## Chemin critique

```
Phase 0 ──┬──────────────────────────────►  (indépendantes)
Phase 1 ──┘   <- à faire tôt, protège la prod

Phase 2.1 (GL moderne) ──► Phase 2.2 (shaders GLSL)

Phase 3.1 (-cdinfo, Fork) ──► Phase 3.2 (Launcher)

Phase 5.1 (étude Fightcade) ──► Phase 5.2 (GGPO)
```

**Total : ~2 à 3 mois** selon la profondeur du netplay.

## Inconnues à lever avant engagement

1. Licence exacte de fightcade-fbneo
2. Vitalité d'Open Kaillera
3. Ampleur réelle du changement de modèle de données CD dans `games.db`
