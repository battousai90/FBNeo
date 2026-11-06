#!/bin/bash
# Série de tests des records (.hi) de FBNeo, sur de vraies ROMs.
#
# Chaque jeu tourne deux fois en démonstration, sans personne aux commandes,
# dans un dossier jetable (HOME, données, affichage virtuel, son muet, aucune
# manette). Contrôles :
#   1. 1re session : le .hi est écrit ;
#   2. 2e session : la plus grande zone (la table des records) est rechargée
#      et confirmée en mémoire (« Applied Hi Score Memory Range N ») ;
#   3. 2e session : FBNeo accepte d'écrire (pas de « -NOT- ok to write ») et
#      le .hi est réécrit ;
#   4. si inject/<jeu>.py existe : un record posé entre les deux sessions
#      ressort intact.
# Un échec à un seul contrôle fait échouer le jeu, et la série (code 1).
#
# Usage : tests/hiscore/run.sh [jeu ...]       (défaut : tous ceux de games.txt)
#   FBNEO     l'émulateur à tester         (défaut : ./fbneo à la racine du dépôt)
#   ROMS      dossier des ROMs arcade      (obligatoire, lu en lecture seule)
#   HISCORE_DAT  hiscore.dat à utiliser    (défaut : celui que reçoivent les joueurs
#                de Bootcade, téléchargé depuis le serveur de scores)
#   SECS      durée d'une session          (défaut : 40)
#   OUT       résultats et journaux        (défaut : dossier temporaire, affiché)
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
FBNEO="${FBNEO:-$ROOT/fbneo}"
ROMS="${ROMS:?ROMS : dossier des ROMs arcade}"
SECS="${SECS:-40}"
OUT="${OUT:-$(mktemp -d /tmp/fbneo-hiscore-tests.XXXXXX)}"
[ -x "$FBNEO" ] || { echo "émulateur introuvable : $FBNEO" >&2; exit 2; }
for t in Xvfb bwrap python3; do command -v $t >/dev/null || { echo "$t manquant" >&2; exit 2; }; done
mkdir -p "$OUT"
if [ -z "${HISCORE_DAT:-}" ]; then
    HISCORE_DAT="$OUT/hiscore.dat"
    curl -fsSL --max-time 60 https://scores.bootcade.duckdns.org/api/hiscore-dat -o "$HISCORE_DAT" \
        || { echo "hiscore.dat introuvable (serveur de scores)" >&2; exit 2; }
fi

if [ $# -gt 0 ]; then GAMES=("$@")
else mapfile -t GAMES < <(grep -v '^#' "$HERE/games.txt" | cut -f1 | grep .); fi

DISPLAY_NUM=":$((90 + RANDOM % 9))"
Xvfb "$DISPLAY_NUM" -screen 0 1280x1024x24 >/dev/null 2>&1 & XPID=$!
trap 'kill $XPID 2>/dev/null' EXIT
sleep 1

session() {   # session JEU DATA LOG : une session de démo, FBNeo lancé depuis DATA
    ( cd "$2" && HOME="$W/home" XDG_DATA_HOME="$W/home/.local/share" XDG_CONFIG_HOME="$W/home/.config" \
      DISPLAY=$DISPLAY_NUM SDL_AUDIODRIVER=dummy \
      bwrap --dev-bind / / --tmpfs /dev/input --ro-bind "$ROMS" "$ROMS" --die-with-parent \
      timeout --foreground -s INT -k 30 "$SECS" "$FBNEO" "$1" ) > "$3" 2>&1
    # timeout attend la fin de FBNeo lui-même : le .hi s'écrit pendant sa
    # sortie, le regarder avant donnait de faux « pas écrit ».
}
mtime() { stat -c %Y "$1" 2>/dev/null || echo none; }

fail=0; passed=0
for game in "${GAMES[@]}"; do
    W=$(mktemp -d); DATA="$W/home/.local/share/fbneo"; HI="$DATA/support/hiscores/$game.hi"
    mkdir -p "$DATA/config/games" "$DATA/support/hiscores" "$W/home/.config"
    cp "$HISCORE_DAT" "$DATA/support/hiscores/hiscore.dat" || exit 2
    printf 'EnableHiscores 1\nszAppRomPaths[0] %s/\n' "$ROMS" > "$DATA/config/fbneo.ini"
    problems=()

    session "$game" "$DATA" "$OUT/$game.1.log"
    [ -s "$HI" ] || problems+=("1re session : pas de .hi")
    inject="$HERE/inject/$game.py"
    if [ -s "$HI" ] && [ -f "$inject" ]; then python3 "$inject" write "$HI"; fi
    before=$(mtime "$HI"); sleep 1.1

    session "$game" "$DATA" "$OUT/$game.2.log"
    if [ -s "$HI" ] && [ "${#problems[@]}" -eq 0 ]; then
        # La plus grande zone de l'entrée : celle qui porte la table.
        big=$(python3 "$HERE/biggest_range.py" "$HISCORE_DAT" "$game")
        if [ -z "$big" ]; then problems+=("pas d'entrée dans hiscore.dat")
        elif ! grep -q "Applied Hi Score Memory Range $big " "$OUT/$game.2.log"; then
            problems+=("2e session : table (zone $big) jamais rechargée"); fi
        grep -q -- "-NOT- ok to write" "$OUT/$game.2.log" && problems+=("2e session : écriture refusée")
        [ "$(mtime "$HI")" != "$before" ] || problems+=("2e session : .hi non réécrit")
        if [ -f "$inject" ] && ! python3 "$inject" check "$HI"; then problems+=("record posé perdu"); fi
    fi
    cp "$HI" "$OUT/$game.hi" 2>/dev/null
    vol=$(grep -c "not blocking the save" "$OUT/$game.2.log")
    if [ "${#problems[@]}" -eq 0 ]; then
        echo "ok    $game${vol:+$( [ "$vol" -gt 0 ] && echo "  ($vol zone(s) volatile(s))")}"; passed=$((passed + 1))
    else
        echo "ÉCHEC $game : $(IFS=';'; echo "${problems[*]}")"; fail=1
    fi
    rm -rf "$W"
done
echo "$passed / ${#GAMES[@]} jeux ok — journaux : $OUT"
exit $fail
