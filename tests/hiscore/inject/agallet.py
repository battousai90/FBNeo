"""Air Gallet : un record inventé (990 000, « GIL ») glissé dans le .hi entre
les deux sessions. S'il ressort tel quel de la 2e session, la table chargée a
tenu en mémoire et le fichier a été réécrit : le cycle complet d'un record.

  agallet.py write FICHIER   pose le record
  agallet.py check FICHIER   code 0 s'il y est encore
Format (hi2txt agallet.xml) : score de tête 4 octets, 6 octets inconnus,
10 scores de 4 octets, 10 noms de 3 caractères, 10 niveaux."""
import sys

RECORD = bytes.fromhex("00990000")
action, path = sys.argv[1], sys.argv[2]
data = bytearray(open(path, "rb").read())
if action == "write":
    data[0:4] = RECORD
    data[10:14] = RECORD
    data[50:53] = b"GIL"
    open(path, "wb").write(data)
else:
    sys.exit(0 if data[0:4] == RECORD and data[10:14] == RECORD and data[50:53] == b"GIL" else 1)
