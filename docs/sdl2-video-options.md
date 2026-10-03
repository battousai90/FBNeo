# Options vidéo du frontend SDL2

Document de référence pour le launcher : chaque option ajoutée au frontend SDL2
(`./fbneo`), avec sa plage, sa valeur par défaut, sa clé dans `fbneo.ini` et son effet.

Toutes ces options s'appliquent au renderer SDL2 (`nVidSelect = 0`, le défaut).
Le module OpenGL historique (`vid_sdl2opengl.cpp`) ne les prend pas en charge.

## Vue rapide

| Option ligne de commande | Valeurs | Défaut | Clé `fbneo.ini` | Effet |
|---|---|---|---|---|
| `-softfx <n>` | `-1` (off) ou `0` à `37` | `-1` | `nVidSoftFX` | Filtre logiciel (scaler) appliqué à chaque image |
| `-softfx list` | | | | Liste les filtres |
| `-scanlines` | | `0` | `bVidScanlines` | Une ligne sur deux assombrie |
| `-scanintensity <n>` | `0` à `255` | `191` | `nVidScanIntensity` | Intensité des lignes sombres (même valeur pour B, G, R) |
| `-list-video-json` | | | | Sortie JSON pour le launcher (voir ci-dessous) |
| `-rgbmask <n>` | `0` à `10` | `0` | `nVidRGBMask` | Masque de sous-pixels RGB |
| `-rgbmask list` | | | | Liste les motifs |
| `-stretch` | | `0` | `bVidFullStretch` | Image étirée sur toute la fenêtre |
| `-internalres <n>` | `1` à `4` | `1` | `nVidInternalRes` | Image dessinée à n× avant la fenêtre |
| `-renderer <nom>` | `opengl`, `opengles2`, `software` | vide (défaut SDL) | `szVidRenderer` | Backend de rendu SDL |

Valeur par défaut « vide » pour `szVidRenderer` : SDL choisit lui-même le backend.

## Détail par option

### `-softfx <n>` et `-softfx list`

- `n` est l'index du filtre dans la liste (`0` à `37`). `-1` désactive le filtre.
- `-softfx list` affiche les 38 filtres (sortie texte, code 0, sans message d'usage) avec leur index et leur zoom (`x2`, `x3`…).
  Quatre filtres sont marqués `[needs x86 asm build]` : Eagle (5), hq2x (14),
  hq3x (15) et hq4x (16). Ils ne sont pas disponibles dans ce build.
- Un filtre qui ne prend pas la profondeur du jeu affiche le message
  `SoftFX filter ... is not available for this build and colour depth, using plain output`
  et l'image reste normale.
- Certains filtres n'existent qu'en 16 bits (2xPM, 2xSaI, SuperScale, hq3xS VBA, EPX, DDT3x…).
  Dans ce cas, l'image passe automatiquement en 16 bits pour ce jeu, sauf si le jeu est déjà en 16 bits.
- Les filtres CRT (35 à 37) n'existent qu'en 32 bits : ils sont refusés sur un jeu 16 bits.
- Le zoom du filtre (`x2`, `x3`, `x4`) agrandit la texture avant la fenêtre.
  C'est pour cela qu'il est inutile de combiner `-internalres` avec un filtre zoomé pour un gain de qualité.

### `-scanlines` et `-scanintensity <n>`

- `-scanlines` assombrit une ligne sur deux de l'image.
- `-scanintensity` règle l'intensité : `191` (`0xBF`, 75 %) par défaut, comme le Win32. La même valeur est appliquée aux trois couleurs.
- Dans `fbneo.ini`, la clé `nVidScanIntensity` est stockée sous forme d'entier `0xBBGGRR` : `191` correspond à `12566463` (`0x00BFBFBF`), et `-scanintensity N` écrit `N * 0x010101`.
- Appliqué sur l'image finale, donc avec ou sans SoftFX.

### `-rgbmask <n>` et `-rgbmask list`

- `n` de `1` à `10` choisit un motif (voir `-rgbmask list`). `0` désactive le masque.
- Motifs : 18x10 large round, 12x10 large ellipsoid, 10x6 large dot, 9x10 ellipsoid,
  8x8 mame rgbtiny, 6x8 rgb pattern, 4x6 rgb pattern, 4x4 mame rgbtiny,
  4x4 rgb pattern, 3x1 aperture grille. Ce sont les tables du blitter Direct3D.
- Le mélange est une multiplication. Les sous-pixels éteints sont noirs,
  et l'image peut paraître sombre. Les presets, modes de mélange et atténuation du Win32 ne sont pas portés.
- Les composantes du motif sont lues dans l'ordre B, G, R. Si les couleurs semblent inversées,
  c'est à vérifier visuellement.

### `-stretch`

- Étire l'image sur toute la fenêtre, sans conserver le ratio d'aspect.
- Chemin de placement manuel : voir « Restrictions » ci-dessous.

### `-internalres <n>`

- `n` de `1` à `4`. `1` ne change rien.
- Dessine l'image dans une cible de rendu de `n` fois la taille d'affichage, puis la place sur la fenêtre
  en gardant le ratio (ou en plein écran avec `-stretch`).
- Avec SoftFX, la texture est déjà zoomée : `-internalres` agrandit encore, sans gain de netteté visible.
- Demande un backend qui sait faire des cibles de rendu. Si la création échoue, un message est affiché
  (`internalres N not available: ...`) et le rendu reste sans cible.

### `-renderer <nom>`

- Valeurs acceptées : `opengl`, `opengles2`, `software`. Ce sont les seuls backends de SDL2 sur cette machine
  (SDL 2.32.10). Il n'y a pas de backend Vulkan dans SDL2.
- Un nom inconnu n'est pas refusé : SDL retombe sur son choix par défaut.
- Agit sur tous les renderers SDL créés après le démarrage (menu compris).

## Priorité : ligne de commande et `fbneo.ini`

- Toutes les options de la ligne de commande gagnent sur `fbneo.ini`. La ligne de commande est relue après le chargement du fichier.
- Un launcher peut donc passer ses choix à chaque lancement sans toucher au fichier.
- Une valeur donnée sur la ligne de commande n'est pas sauvegardée dans `fbneo.ini`.
- Un launcher peut donc passer ses choix à chaque lancement sans toucher au fichier.
- Pour `-renderer`, la chaîne est copiée telle quelle (31 caractères au maximum).

## Sortie pour le launcher : `-list-video-json`

`./fbneo -list-video-json` écrit un JSON sur stdout puis quitte (code 0), avant toute autre impression.
Contenu :

- `softfx` : pour chaque filtre `index`, `name`, `zoom`, `available` (dans ce build) et `depths` (16 et/ou 32).
- `rgbmask` : `index` (1 à 10) et `name`.
- `renderers` : les backends SDL disponibles sur la machine.
- `ranges` : `min`, `max` et `default` de `softfx`, `rgbmask`, `internalres`, `scanintensity`, et la valeur par défaut de `renderer`.

Le launcher doit lire ce JSON plutôt que coder les valeurs en dur : les filtres disponibles dépendent du build.

## Restrictions et interactions

- `-stretch` et `-internalres` ne s'appliquent pas aux jeux tournés (`BDF_ORIENTATION_VERTICAL` avec rotation)
  ni aux jeux retournés (`BDF_ORIENTATION_FLIPPED`) : le placement reste celui de SDL.
- `-scanlines` et `-rgbmask` sont appliqués dans le renderer SDL2 ; ils ne concernent pas le module OpenGL historique.
- Les options ne sont actives qu'avec un jeu chargé (pas dans le menu).
- `-softfx list` et `-rgbmask list` affichent la liste puis le message d'usage et quittent.
  Un launcher ne doit pas les utiliser comme sortie à parser.

## Exemples de lignes de commande pour un launcher

```
./fbneo -softfx 25 -scanlines -renderer opengl <romname>
./fbneo -softfx 12 <romname>
./fbneo -rgbmask 7 -stretch <romname>
./fbneo -internalres 2 -renderer software <romname>
```

## Limitations connues

- Vulkan : non disponible. Il faudrait migrer le frontend vers SDL3 ou écrire un backend dédié.
- Eagle (filtre 5), hq2x/3x/4x (14 à 16) : en assembleur uniquement, non portés.
- Presets RGB du Win32 (modes de mélange, atténuation) : non exposés.
- Les effets (scanlines, RGB) et `-internalres` n'ont pas été contrôlés visuellement pour l'instant.
