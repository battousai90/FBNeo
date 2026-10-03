# Prompt : créer fbneo-launcher

Copier tout ce qui suit dans une nouvelle session Claude Code, ouverte dans le dossier
où le launcher doit vivre (par exemple `~/DEV/fbneo-launcher`, un dépôt git neuf).

---

Tu vas créer **fbneo-launcher**, une application de bureau Linux qui sert de lanceur
pour l'émulateur FinalBurn Neo (FBNeo) compilé en frontend SDL2. Le launcher ne modifie
pas FBNeo : il construit une ligne de commande et lance `fbneo`.

## Ce que fait FBNeo (contrat à respecter)

Binaire : `fbneo`, construit depuis le dépôt `~/DEV/FBNeo` (`make sdl2`). Le launcher doit
recevoir le chemin de ce binaire dans ses réglages, ne pas le coder en dur.

Lancer un jeu : `fbneo [options] <romname>`, où `<romname>` est le nom du zip sans extension
(`pacman`, `1942`, `gng`…). Un jeu se lance depuis le dossier de ROMs configuré dans FBNeo.

Options vidéo et de rendu ajoutées au frontend SDL2 (toutes documentées dans
`~/DEV/FBNeo/docs/sdl2-video-options.md` — le lire en premier) :

- `-softfx <n>` : filtre logiciel, `n` de `-1` (off) à `37`.
- `-scanlines` : une ligne sur deux assombrie.
- `-scanintensity <0-255>` : intensité des lignes (défaut `191`).
- `-rgbmask <n>` : masque RGB, `n` de `0` (off) à `10`.
- `-stretch` : image étirée sur la fenêtre.
- `-internalres <1-4>` : rendu intermédiaire N×.
- `-renderer <opengl|opengles2|software>` : backend SDL.
- Déjà existantes : `-integerscale`, `-windowscale <1-20>`, `-nearest`, `-linear`,
  `-best`, `-novsync`, `-fullscreen`, `-menu`, `-autosave`, `-joy`, `-cd`.

**Règle de priorité** : toute option passée sur la ligne de commande gagne sur
`~/.local/share/fbneo/config/fbneo.ini`. Le launcher passe donc **toujours** ses choix
en ligne de commande et **n'écrit pas** dans `fbneo.ini`, sauf pour lire les chemins de ROMs.

**Découverte des options** : la commande suivante écrit un JSON sur stdout, avant toute autre
sortie, et quitte avec le code 0 :

    fbneo -list-video-json

Le JSON contient :
- `softfx` : pour chaque filtre `index`, `name`, `zoom`, `available` (dans ce build) et `depths`
  (`16`, `32` ou les deux). **Ne jamais coder la liste en dur** : les filtres disponibles dépendent du build.
- `rgbmask` : `index` (1 à 10) et `name`.
- `renderers` : les backends SDL présents sur la machine.
- `ranges` : `min`, `max`, `default` pour `softfx`, `rgbmask`, `internalres`, `scanintensity`,
  et `default` pour `renderer`.

Les listes texte `fbneo -softfx list` et `fbneo -rgbmask list` existent aussi, mais le
launcher doit utiliser le JSON.

**Chemins de ROMs** : dans `fbneo.ini`, les clés `szAppRomPaths[0]` à `szAppRomPaths[N]`
(`N` jusqu'à 4 au moins) donnent les dossiers à scanner. Les lignes ont la forme
`szAppRomPaths[0] /chemin/avec des espaces/`. Le chemin est tout ce qui suit le premier espace
après le crochet fermant, sans les guillemets.

**Titres des jeux** : le fichier `gamelist.txt` à la racine du dépôt FBNeo liste les jeux
supportés avec leur titre et leur statut (`NW` = non fonctionnel, `X` = exclu du build).
Le launcher peut s'en servir pour afficher les titres et masquer les jeux non fonctionnels
(option).

**Sortie console** : FBNeo écrit beaucoup de lignes sur stdout et stderr (chargement des ROMs,
entrées). Le launcher doit capturer la sortie dans un journal, pas la montrer telle quelle.

## Ce que le launcher doit faire

1. **Bibliothèque** : lister les ROMs des dossiers `szAppRomPaths`, avec recherche, filtre
   (par système si possible, à partir des noms de zip ou de `gamelist.txt`), et un marqueur
   pour les jeux non fonctionnels.
2. **Réglages globaux** : une page de réglages qui couvre toutes les options ci-dessus, avec les
   plages et valeurs par défaut lues dans le JSON. Les filtres proposés sont ceux que le JSON
   déclare disponibles. Un bouton « valeurs par défaut » rétablit les défauts du JSON.
3. **Réglages par jeu** (optionnel, en second temps) : surcharger quelques options pour un jeu donné
   (par exemple un filtre SoftFX pour un shmup, aucun pour un jeu de combat).
4. **Lancement** : construire la ligne de commande à partir des réglages (global + jeu), la
   **afficher** avant lancement (aperçu copiable), lancer `fbneo` comme processus enfant, et
   afficher l'état (en cours, terminé, code de sortie). Plusieurs lancements simultanés ne sont
   pas nécessaires.
5. **Journal** : garder la sortie de la dernière session par jeu, consultable depuis l'interface.
6. **Configuration du launcher** : chemin du binaire `fbneo`, dossier de ROMs (si différent de
   `fbneo.ini`), profil des réglages, stockés dans `~/.config/fbneo-launcher/`.
7. **Erreurs** : binaire introuvable, ROM absente, option refusée par FBNeo (code de retour non nul
   ou message d'usage sur stdout). Les messages doivent être clairs, en français.

## Choix techniques (par défaut, modifiables)

- **Python 3** avec **PySide6** (Qt). Si PySide6 pose problème, Tkinter est acceptable.
- Structure : un paquet `fbneo_launcher/` avec séparation claire entre : lecture du JSON et des
  chemins (`fbneo.py`), modèle de réglages et construction de la ligne de commande (`command.py`,
  sans dépendance graphique, testable), bibliothèque de ROMs (`library.py`), interface (`ui/`).
- **Tests** : `pytest`. Tester au minimum la construction de la ligne de commande à partir des
  réglages, la lecture du JSON (avec un JSON d'exemple dans `tests/`), et la lecture de
  `szAppRomPaths` dans un `fbneo.ini` de test.
- Pas de dépendance réseau. Pas de télémétrie.

## Règles de travail

- Lire `~/DEV/FBNeo/docs/sdl2-video-options.md` et `~/DEV/FBNeo/CLAUDE.md` avant de coder.
- Ne jamais modifier `~/DEV/FBNeo` depuis cette session. Le launcher est un projet séparé.
- Ne pas ajouter de fonctionnalité de triche, de recherche de cheats, de NVRAM ou de cartes mémoire
  (le fork refuse ces fonctions pour protéger l'intégrité des scores Bootcade).
- **Commits** : messages en français, **aucune ligne d'attribution** (`Co-Authored-By`,
  `Generated with Claude Code`…). Le message s'arrête au corps du commit.
- Committer par étape logique, avec des messages clairs. Ne pas pousser sur un dépôt distant sans demande.

## Ce qu'il faut me rendre

- Un premier jalon fonctionnel : lecture du JSON et des ROMs, réglages globaux, ligne de commande
  affichée et lancement. Les réglages par jeu et le journal viennent ensuite.
- Un `README.md` qui dit comment lancer le launcher et comment le pointer vers `fbneo`.
- Les tests qui passent, et la liste de ce qui n'a pas été vérifié visuellement.
