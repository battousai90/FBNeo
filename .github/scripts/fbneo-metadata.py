#!/usr/bin/env python3
"""Extrait TOUT ce que FinalBurn Neo sait de chaque jeu, depuis sa source.

Pourquoi ce fichier existe : les DAT decrivent des ROMs (noms, tailles,
sommes de controle), pas des jeux. Le genre, la serie, le nombre de joueurs
et le fait qu'un jeu gere les meilleurs scores ne sont ecrits nulle part
dans un DAT. Ils sont en revanche declares par chaque pilote FBNeo, dans sa
structure `BurnDriver`. C'est la source, elle est sur le disque, et elle est
la meme que celle de l'emulateur qu'on distribue.

Sortie : un JSON { "<nom de rom>": { ... } } depose a cote des DAT, que
generate-catalog-data.py fusionne dans catalog-data.json. Le catalogue, la
page d'un jeu, le service de scores et le lanceur lisent donc tous la meme
chose, extraite une seule fois.

    ./fbneo-metadata.py ~/DEV/FBNeo/src fbneo-meta.json

Disposition d'une structure BurnDriver (src/burn/burn.h) ; les champs sont
pris par POSITION, ce qui est fiable la ou compter les chaines ne l'est pas
(un clone a un parent, donc une chaine de plus) :

     0 nom court      1 parent        2 rom de carte   3 nom complet (rare)
     4 date           5 titre         6 commentaire    7 fabricant
     8 systeme        9-12 variantes larges (inutilisees)
    13 drapeaux      14 joueurs      15 materiel      16 genre
    17 famille
"""
import json
import os
import re
import sys

# Libelles repris MOT POUR MOT de FBNeo (src/burner/win32/string.rc), pour
# que le catalogue dise ce que dit l'emulateur, et pas une traduction maison.
GENRE = {
    "GBF_HORSHOOT": "Shooter / Horizontal / Sh'mup",
    "GBF_VERSHOOT": "Shooter / Vertical / Sh'mup",
    "GBF_SCRFIGHT": "Fighting / Beat 'em Up",
    "GBF_VSFIGHT": "Fighting / Versus",
    "GBF_BIOS": "BIOS",
    "GBF_BREAKOUT": "Breakout",
    "GBF_CASINO": "Casino",
    "GBF_BALLPADDLE": "Ball & Paddle",
    "GBF_MAZE": "Maze",
    "GBF_MINIGAMES": "Mini-Games",
    "GBF_PINBALL": "Pinball",
    "GBF_PLATFORM": "Platformer",
    "GBF_PUZZLE": "Puzzle",
    "GBF_QUIZ": "Quiz",
    "GBF_SPORTSMISC": "Sports",
    "GBF_SPORTSFOOTBALL": "Sports / Football",
    "GBF_MISC": "Misc",
    "GBF_MAHJONG": "Mahjong",
    "GBF_RACING": "Racing",
    "GBF_SHOOT": "Shooter",
    "GBF_MULTISHOOT": "Shooter / Multi-direction / Sh'mup",
    "GBF_RUNGUN": "Run 'n Gun",
    "GBF_STRATEGY": "Strategy",
    "GBF_ACTION": "Action",
    "GBF_RPG": "RPG",
    "GBF_SIM": "Simulator",
    "GBF_ADV": "Adventure",
    "GBF_CARD": "Card Game",
    "GBF_BOARD": "Board Game",
    "GBF_VECTOR": "Vector",
}
FAMILY = {
    "FBF_MSLUG": "Metal Slug",
    "FBF_SF": "Street Fighter",
    "FBF_KOF": "The King of Fighters",
    "FBF_DSTLK": "Darkstalkers",
    "FBF_FATFURY": "Fatal Fury",
    "FBF_SAMSHO": "Samurai Shodown",
    "FBF_19XX": "19XX",
    "FBF_SONICWI": "Aero Fighters",
    "FBF_PWRINST": "Power Instinct",
    "FBF_SONIC": "Sonic the Hedgehog",
    "FBF_DONPACHI": "DonPachi",
    "FBF_MAHOU": "Mahou Daisakusen",
}
# Ce que chaque drapeau raconte du jeu. `BDF_GAME_WORKING` et
# `BDF_HISCORE_SUPPORTED` sont les deux qui comptent vraiment ici : le
# second est la reponse de FBNeo a « ce jeu gere-t-il les meilleurs
# scores », question distincte de « le service Bootcade le classe-t-il ».
FLAG = {
    "BDF_GAME_WORKING": "working",
    "BDF_ORIENTATION_FLIPPED": "flipped",
    "BDF_ORIENTATION_VERTICAL": "vertical",
    "BDF_BOARDROM": "boardrom",
    "BDF_CLONE": "clone",
    "BDF_BOOTLEG": "bootleg",
    "BDF_PROTOTYPE": "prototype",
    "BDF_HACK": "hack",
    "BDF_HOMEBREW": "homebrew",
    "BDF_DEMO": "demo",
    "BDF_HISCORE_SUPPORTED": "hiscore",
    "BDF_16BIT_ONLY": "16bit",
    "BDF_32BIT_ONLY": "32bit",
    "BDF_RUNAHEAD_DRAWSYNC": "runahead_drawsync",
    "BDF_RUNAHEAD_DISABLED": "runahead_disabled",
}

DRIVER = re.compile(r"struct\s+BurnDriver\w*\s+\w+\s*=\s*\{(.*?)\n\}\s*;", re.S)


def strip_dead_code(text):
    """Retire ce que le compilateur ne voit jamais : commentaires et #if 0.

    Les fichiers de pilotes en contiennent : des pilotes de test mis de
    cote, des variantes desactivees. Les lire produirait des jeux qui
    n'existent pas, et surtout un pilote desactive peut porter le meme nom
    qu'un pilote actif : le dernier lu ecraserait alors le bon. Aucun de
    ces 34 fantomes n'apparait aujourd'hui dans un DAT, mais rien ne le
    garantit demain, et une donnee fausse est plus couteuse a debusquer
    qu'a empecher.

    Les chaines sont respectees : un titre peut contenir « // » ou « /* ».
    """
    out, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        if c == '"':
            j = i + 1
            while j < n:
                if text[j] == "\\":
                    j += 2
                    continue
                if text[j] == '"':
                    break
                j += 1
            out.append(text[i:j + 1])
            i = j + 1
        elif c == "/" and i + 1 < n and text[i + 1] == "*":
            j = text.find("*/", i + 2)
            j = n if j == -1 else j + 2
            out.append(" " * (j - i))      # meme longueur : les numeros de ligne tiennent
            i = j
        elif c == "/" and i + 1 < n and text[i + 1] == "/":
            j = text.find("\n", i)
            j = n if j == -1 else j
            out.append(" " * (j - i))
            i = j
        else:
            out.append(c)
            i += 1
    text = "".join(out)

    # `#if 0 ... #else ... #endif` : la branche gardee est celle que le
    # compilateur garderait, donc rien avant le #else et tout apres.
    kept, stack = [], []
    for line in text.split("\n"):
        head = line.lstrip()
        if re.match(r"#\s*if\s+0\b", head):
            stack.append("off")
            kept.append("")
            continue
        if stack:
            if re.match(r"#\s*if", head):
                stack.append("nested")
                kept.append("")
                continue
            if re.match(r"#\s*else", head) and len(stack) == 1:
                stack[0] = "on"
                kept.append("")
                continue
            if re.match(r"#\s*endif", head):
                stack.pop()
                kept.append("")
                continue
            kept.append(line if stack[-1] == "on" else "")
            continue
        kept.append(line)
    return "\n".join(kept)


def fields(body):
    """Decoupe le corps de la structure sur les virgules de premier niveau.

    Ni split(',') ni une expression reguliere ne suffisent : un titre peut
    contenir une virgule, et les initialiseurs contiennent des parentheses.
    """
    out, buf, depth, i = [], [], 0, 0
    while i < len(body):
        c = body[i]
        if c == '"':                      # chaine : on avale jusqu'au guillemet
            buf.append(c)
            i += 1
            while i < len(body):
                buf.append(body[i])
                if body[i] == "\\":
                    i += 1
                    if i < len(body):
                        buf.append(body[i])
                elif body[i] == '"':
                    break
                i += 1
        elif c in "([{":
            depth += 1
            buf.append(c)
        elif c in ")]}":
            depth -= 1
            buf.append(c)
        elif c == "," and depth == 0:
            out.append("".join(buf).strip())
            buf = []
        else:
            buf.append(c)
        i += 1
    out.append("".join(buf).strip())
    return out


def text(field):
    """La valeur d'un champ chaine, ou None. Les `\\0` de fin sont ceux de
    FBNeo, qui colle parfois plusieurs titres dans un meme litteral."""
    field = field.strip()
    if not field.startswith('"'):
        return None
    value = re.sub(r'"\s*"', "", field)          # litteraux colles
    value = value[1:-1] if value.endswith('"') else value[1:]
    value = value.split("\\0")[0]
    value = value.replace('\\"', '"').strip()
    return value or None


def number(field):
    try:
        return int(field.strip(), 0)
    except (TypeError, ValueError):
        return None


def labels(field, table):
    return [table[t] for t in re.findall(r"\b[A-Z]+_[A-Z0-9_]+\b", field or "")
            if t in table]


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else os.path.expanduser("~/DEV/FBNeo/src")
    out_path = sys.argv[2] if len(sys.argv) > 2 else "fbneo-meta.json"

    games, files = {}, 0
    for base, _dirs, names in os.walk(os.path.join(root, "burn", "drv")):
        for name in sorted(names):
            if not name.endswith((".cpp", ".c")):
                continue
            files += 1
            body_text = strip_dead_code(
                open(os.path.join(base, name), encoding="utf-8",
                     errors="replace").read())
            for body in DRIVER.findall(body_text):
                f = fields(body)
                if len(f) < 18:
                    continue
                rom = text(f[0])
                if not rom:
                    continue
                flags = [FLAG[t] for t in re.findall(r"\bBDF_[A-Z0-9_]+\b", f[13])
                         if t in FLAG]
                entry = {
                    "parent": text(f[1]),
                    "board": text(f[2]),
                    "year": text(f[4]),
                    "title": text(f[5]),
                    "comment": text(f[6]),
                    "manufacturer": text(f[7]),
                    "system": text(f[8]),
                    "players": number(f[14]),
                    "hardware": (re.findall(r"\bHARDWARE_[A-Z0-9_]+\b", f[15]) or [None])[-1],
                    "genre": labels(f[16], GENRE),
                    "family": labels(f[17], FAMILY),
                    "flags": flags,
                    "hiscore": "hiscore" in flags,
                    "working": "working" in flags,
                }
                # Les champs vides ne sont pas ecrits : le fichier est lu par
                # chaque visiteur du catalogue, et 25 000 `null` pesent.
                games[rom] = {k: v for k, v in entry.items() if v not in (None, [], False)}

    with open(out_path, "w", encoding="utf-8") as fh:
        json.dump(games, fh, ensure_ascii=False, sort_keys=True,
                  separators=(",", ":"))
    hi = sum(1 for g in games.values() if g.get("hiscore"))
    gen = sum(1 for g in games.values() if g.get("genre"))
    print(f"{len(games)} jeux lus dans {files} fichiers "
          f"({gen} avec genre, {hi} annonces compatibles hiscore) -> {out_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
