#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Build a Game Boy cartridge library in the shape the card's NES one has.

The NES library on the card (`/root/media/nes`) is one folder per first
character of the title, one zip per game, the region tags gone from the names,
and nothing over what TinyDesk's Files app works with (TDESK-017: 64 entries a
folder with ".." included, so 63 files, and a name of at most 47 bytes).  It
was split further where a letter outgrew that -- B into `Ba` and `Be-Bz`, S
into `Sa-Sp` and `Sq-Sz` -- and its largest folder holds 61 files.  This builds
the same thing for Game Boy and Game Boy Color together.

    tools/make_gb_library.py --to DIR <gameboy> <gbc>
    tools/make_gb_library.py --dry-run --to DIR <gameboy> <gbc>

A title both sets have (Aladdin, Wario Land II and 27 more) is two different
games, so both are kept: the Color one under the plain title, the monochrome
one with ` (GB)` after it.  Within one set, a title released in several
regions keeps the first by USA, then Europe, then World, then Japan.

The zips are copied unchanged; the player inflates them, so a name is the only
thing this decides.
"""

import argparse
import collections
import os
import re
import shutil
import sys

# TinyDesk's Files app: 64 entries a folder with ".." included, 47 bytes a name.
MAX_PER_FOLDER = 61
MAX_NAME = 47
EXT = ".zip"

# The titles that are still over the limit once their tags are gone.  Each is
# shortened by hand, the way the NES library's 32 were: cut at a word boundary
# where that keeps the game's own name, and at the middle where the tail is the
# part that tells one game from another.
RENAMES = {
    "Addams Family, The - Pugsley's Scavenger Hunt":
        "Addams Family - Pugsley's Scavenger Hunt",
    "Adventures of Rocky and Bullwinkle and Friends, The":
        "Adventures of Rocky and Bullwinkle",
    "Arcade Classic No. 1 - Asteroids & Missile Command":
        "Arcade Classic 1 - Asteroids & Missile",
    "Arcade Classic No. 2 - Centipede & Millipede":
        "Arcade Classic 2 - Centipede & Millipede",
    "Arcade Classics - Super Breakout & Battlezone":
        "Arcade Classics - Super Breakout",
    "Austin Powers - Welcome to My Underground Lair!":
        "Austin Powers - Underground Lair",
    "Beauty and the Beast - A Board Game Adventure":
        "Beauty and the Beast - Board Game",
    "Bill & Ted's Excellent Game Boy Adventure - A Bogus Journey!":
        "Bill & Ted's Excellent Game Boy Adventure",
    "Championship Motocross 2001 featuring Ricky Carmichael":
        "Championship Motocross 2001",
    "Cubix - Robots for Everyone - Race 'n Robots":
        "Cubix - Race 'n Robots",
    "David Crane's The Rescue of Princess Blobette":
        "David Crane's Rescue of Princess Blobette",
    "Deja Vu I & II - The Casebooks of Ace Harding":
        "Deja Vu I & II - Ace Harding",
    "Dragon Warrior Monsters 2 - Tara's Adventure":
        "Dragon Warrior Monsters 2 - Tara",
    "E.T. - The Extra-Terrestrial - Digital Companion":
        "E.T. - Digital Companion",
    "E.T. - The Extra-Terrestrial - Escape from Planet Earth":
        "E.T. - Escape from Planet Earth",
    "E.T. - The Extra-Terrestrial and the Cosmic Garden":
        "E.T. - The Cosmic Garden",
    "Flintstones, The - King Rock Treasure Island":
        "Flintstones - King Rock Treasure Island",
    "Harley-Davidson Motor Cycles - Race Across America":
        "Harley-Davidson - Race Across America",
    "Hit the Ice - VHL - The Official Video Hockey League":
        "Hit the Ice - Video Hockey League",
    "Hunchback of Notre Dame, The - 5 Foolishly Fun Topsy Turvy Games":
        "Hunchback of Notre Dame, The",
    "Ken Griffey Jr. Presents Major League Baseball":
        "Ken Griffey Jr. - Major League Baseball",
    "Looney Tunes 2 - Tasmanian Devil in Island Chase":
        "Looney Tunes 2 - Island Chase",
    "Microsoft - The 6 in 1 Puzzle Collection Entertainment Pack":
        "Microsoft - 6 in 1 Puzzle Collection",
    "Midway Presents Arcade Hits - Joust & Defender":
        "Arcade Hits - Joust & Defender",
    "Midway Presents Arcade Hits - Moon Patrol & Spy Hunter":
        "Arcade Hits - Moon Patrol & Spy Hunter",
    "MTV Sports - Skateboarding Featuring Andy MacDonald":
        "MTV Sports - Skateboarding",
    "Ninja JaJaMaru - The Great World Adventure DX":
        "Ninja JaJaMaru - World Adventure DX",
    "Pokemon - Yellow Version - Special Pikachu Edition":
        "Pokemon - Yellow Version",
    "Powerpuff Girls, The - Paint the Townsville Green":
        "Powerpuff Girls, The - Townsville Green",
    "Ren & Stimpy Show, The - Space Cadet Adventures":
        "Ren & Stimpy - Space Cadet Adventures",
    "Roswell Conspiracies - Aliens, Myths & Legends":
        "Roswell Conspiracies",
    "Sesame Street - The Adventures of Elmo in Grouchland":
        "Sesame Street - Elmo in Grouchland",
    "Simpsons, The - Itchy & Scratchy - Miniature Golf Madness":
        "Simpsons, The - Miniature Golf Madness",
    "Simpsons, The - Night of the Living Treehouse of Horror":
        "Simpsons, The - Treehouse of Horror",
    "Spider-Man 3 - Invasion of the Spider-Slayers":
        "Spider-Man 3 - Spider-Slayers",
    "SpongeBob SquarePants - Legend of the Lost Spatula":
        "SpongeBob SquarePants - Lost Spatula",
    "Sports Illustrated - Championship Football & Baseball":
        "Sports Illustrated - Football & Baseball",
    "Sports Illustrated for Kids - The Ultimate Triple Dare!":
        "Sports Illustrated for Kids - Triple Dare",
    "Square Deal - The Game of Two-Dimensional Poker":
        "Square Deal - Two-Dimensional Poker",
    "Teenage Mutant Ninja Turtles - Fall of the Foot Clan":
        "TMNT - Fall of the Foot Clan",
    "Teenage Mutant Ninja Turtles II - Back from the Sewers":
        "TMNT II - Back from the Sewers",
    "Teenage Mutant Ninja Turtles III - Radical Rescue":
        "TMNT III - Radical Rescue",
    "Tiny Toon Adventures 2 - Montana's Movie Madness":
        "Tiny Toon Adventures 2 - Movie Madness",
    "Walt Disney World Quest - Magical Racing Tour":
        "Disney World Quest - Magical Racing Tour",
    "WDL - World Destruction League - Thunder Tanks":
        "WDL - Thunder Tanks",
    "Winnie the Pooh - Adventures in the 100 Acre Wood":
        "Winnie the Pooh - 100 Acre Wood",
    "Wizardry I - Proving Grounds of the Mad Overlord":
        "Wizardry I - Proving Grounds",
}


def title_of(name):
    """The title with the tags gone, as the NES library's names are."""
    stem = name[:-len(EXT)]
    stem = re.sub(r"\s*[\(\[][^\)\]]*[\)\]]", "", stem)
    return re.sub(r"\s+", " ", stem).strip(" -_")


def region_rank(name):
    """USA first, then Europe, World, Japan, then anything else."""
    tags = " ".join(re.findall(r"\(([^)]*)\)", name))
    for rank, word in enumerate(("USA", "Europe", "World", "Japan")):
        if word in tags:
            return rank
    return 4


def collect(source):
    """One cartridge a title: the best region release of each."""
    found = collections.defaultdict(list)
    for name in sorted(os.listdir(source)):
        if not name.lower().endswith(EXT):
            continue
        found[title_of(name)].append(
            (region_rank(name), name, os.path.join(source, name)))
    return {title: sorted(entries)[0][2] for title, entries in found.items()}


def merge(gb, gbc):
    """(title, path) for every cartridge, the shared titles tagged apart."""
    chosen = []
    shared = []
    for title in sorted(set(gb) | set(gbc)):
        base = RENAMES.get(title, title)
        if title in gb and title in gbc:
            shared.append(title)
            chosen.append((base + " (GB)", gb[title]))
            chosen.append((base, gbc[title]))
        elif title in gbc:
            chosen.append((base, gbc[title]))
        else:
            chosen.append((base, gb[title]))
    return chosen, shared


def folder_key(title):
    first = title[:1].upper()
    return first if first.isalnum() else "#"


def buckets_of(titles):
    """Titles by the first letter and, when it is one, the second letter.

    Only letters make a bucket, so a title whose second character is a digit
    or punctuation stays in its letter's first bucket and the ranges keep the
    shape the NES library's have (`Ba`, `Be-Bz`, `Sa-Sp`, `Sq-Sz`).
    """
    buckets = collections.OrderedDict()
    for title in sorted(titles, key=str.casefold):
        second = title[1:2]
        key = title[:1].upper() + (second.lower() if second.isalpha() else "")
        buckets.setdefault(key, []).append(title)
    return buckets


def pack(titles):
    """One letter's titles as folders of at most MAX_PER_FOLDER, balanced.

    A letter that fits stays one folder.  One that does not is cut into as few
    folders as the limit allows and those folders are kept about the same size,
    which is how the NES library's B (66) became 32 and 34 and its S (85)
    became 44 and 41 rather than one folder filled to the limit.
    """
    buckets = buckets_of(titles)
    wanted = max(1, -(-len(titles) // MAX_PER_FOLDER))
    target = -(-len(titles) // wanted)

    folders = []
    group, lo, hi = [], None, None
    for prefix, items in buckets.items():
        more_to_come = len(folders) < wanted - 1
        if group and (len(group) + len(items) > MAX_PER_FOLDER or
                      (more_to_come and len(group) >= target)):
            folders.append((lo, hi, group))
            group, lo, hi = [], None, None
        lo = lo or prefix
        hi = prefix
        group.extend(items)
    if group:
        folders.append((lo, hi, group))
    return folders


def label(letter, lo, hi, split):
    """`C` for a letter that fits one folder, `Sa-Sp` for a split one."""
    if not split:
        return letter
    return lo if lo == hi else "%s-%s" % (lo, hi)


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--to", required=True, help="destination folder")
    ap.add_argument("--dry-run", action="store_true",
                    help="print the plan, copy nothing")
    ap.add_argument("sources", nargs=2, metavar=("GAMEBOY", "GBC"),
                    help="the MiSTer folders holding the zips")
    args = ap.parse_args()

    gb = collect(args.sources[0])
    gbc = collect(args.sources[1])
    chosen, shared = merge(gb, gbc)

    for title, _ in chosen:
        if len(title) + len(EXT) > MAX_NAME:
            sys.exit("still too long and not in RENAMES: %r" % title)

    by_key = collections.defaultdict(list)
    for title, path in chosen:
        by_key[folder_key(title)].append((title, path))

    plan = []
    for key in sorted(by_key):
        if not key.isalpha():
            plan.append((key, by_key[key]))
            continue
        paths = dict(by_key[key])
        groups = pack([t for t, _ in by_key[key]])
        for lo, hi, group in groups:
            plan.append((label(key, lo, hi, len(groups) > 1),
                         [(t, paths[t]) for t in group]))

    for folder, files in plan:
        names = [title + EXT for title, _ in files]
        if len(names) != len(set(names)):
            sys.exit("two cartridges share a name in %s" % folder)
        if len(files) > MAX_PER_FOLDER:
            sys.exit("%s holds %d, over %d" % (folder, len(files), MAX_PER_FOLDER))
        for name in names:
            if len(name.encode()) > MAX_NAME:
                sys.exit("%r is over %d bytes" % (name, MAX_NAME))

    total = sum(len(files) for _, files in plan)
    biggest = max(len(files) for _, files in plan)
    print("%d cartridges in %d folders, largest %d, %d titles are both a"
          " Game Boy and a Game Boy Color game and carry (GB)"
          % (total, len(plan), biggest, len(shared)))
    for folder, files in plan:
        print("  %-8s %3d" % (folder, len(files)))

    if args.dry_run:
        return

    if os.path.exists(args.to):
        sys.exit("%s already exists; remove it first" % args.to)
    for folder, files in plan:
        target = os.path.join(args.to, folder)
        os.makedirs(target)
        for title, path in files:
            shutil.copy2(path, os.path.join(target, title + EXT))


if __name__ == "__main__":
    main()
