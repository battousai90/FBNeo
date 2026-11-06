"""biggest_range.py HISCORE_DAT JEU : le numéro de la plus grande zone de
l'entrée du jeu dans hiscore.dat (celle qui porte la table des records).
Une entrée : une ou plusieurs lignes « nom: », puis une ligne par zone
« cpu:adresse:octets:début:fin ». Rien n'est affiché si le jeu n'y est pas."""
import re
import sys

path, game = sys.argv[1], sys.argv[2]
names, ranges, entries = [], [], []
for line in open(path, encoding="latin-1"):
    line = line.split(";")[0].strip()
    if re.fullmatch(r"[\w,]+:", line):
        if ranges:
            entries.append((names, ranges))
            names, ranges = [], []
        names += line[:-1].split(",")
    elif re.match(r"^[0-9a-fA-F]+:[0-9a-fA-F]+:[0-9a-fA-F]+:", line):
        ranges.append(int(line.split(":")[2], 16))
if ranges:
    entries.append((names, ranges))
for names, ranges in entries:
    if game in names:
        print(max(range(len(ranges)), key=lambda i: ranges[i]))
        break
