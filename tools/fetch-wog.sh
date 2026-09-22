#!/usr/bin/env bash
# Fetches "In The Wake of Gods" (WoG) for VCMI 1.7 and stages it as a drop-in mods directory, without the VCMI launcher.
# Usage: tools/fetch-wog.sh
#   Output: work/mods/Mods/wake-of-gods. Copy the contents of that Mods directory into /media/fat/vcmi/data/Mods on
#   the MiSTer (or run `scripts/deploy.sh mods`). Nothing is committed or bundled: WoG's mod.json declares no license
#   and is about 0.1 GB. Unlike HotA, WoG needs no sibling mod: its only external references are optional
#   "compatibility" submods for other terrain mods (asphalt-terrain, new-pavilion, newtown-terrains, and HotA's own
#   terrains) that we don't stage, so those specific submods get pruned; the rest of WoG does not depend on them.
#   Staged under its upstream folder name, wake-of-gods, not shortened to "wog": WoG's own submods cross-reference
#   each other by that name (e.g. stackExperience depends on wake-of-gods.creatures), and VCMI derives a mod's id
#   from its folder name, so renaming the folder would break those internal dependencies. scripts/bundle.sh maps
#   the short "wog" launcher name to this folder.
# The download comes from the official VCMI mod repository listing (vcmi-mods-repository, vcmi-1.7.json) and is
# checked against the SHA-256 sum below; if upstream re-releases the mod the check fails, so update the pin after
# looking at the new release.
set -euo pipefail
. "$(dirname "${BASH_SOURCE[0]}")/../scripts/env.sh"

WOG_URL=https://github.com/vcmi-mods/wake-of-gods/releases/download/1.7/wake-of-gods-vcmi-1.7.zip
WOG_SHA256=4e631e04cb45f54a6650da960652138161dc8a5e50241fb91c64a74aad1cbc56

OUT="$WORK/mods"
STAGE="$WORK/wog-stage"

fetch() {	# url sha256 -> $DL/<file>
	local file="$DL/$(basename "$1")"
	if [ ! -f "$file" ] || [ "$(sha256sum "$file" | cut -d' ' -f1)" != "$2" ]; then
		curl -fL --retry 3 -o "$file" "$1"
	fi
	if [ "$(sha256sum "$file" | cut -d' ' -f1)" != "$2" ]; then
		echo "SHA-256 mismatch for $file (expected $2)" >&2
		exit 1
	fi
}

fetch "$WOG_URL" "$WOG_SHA256"

# $OUT/Mods is shared with tools/fetch-hota.sh and any future tools/fetch-*.sh, so only ever touch our own
# subfolder here, never the whole directory. WoG's release zip nests its actual content one level down, under
# "Mods/" (capitalized, matching HotA's own layout) inside the release archive's top folder; we want that
# content directly under Mods/wake-of-gods.
rm -rf "$STAGE"
mkdir -p "$STAGE"
unzip -q "$DL/$(basename "$WOG_URL")" -d "$STAGE"
mkdir -p "$OUT/Mods"
rm -rf "$OUT/Mods/wake-of-gods"
mv "$STAGE"/* "$OUT/Mods/wake-of-gods"
rm -rf "$OUT/Mods/wake-of-gods/.github" "$OUT/Mods/wake-of-gods/screenshots" "$STAGE"

python3 - "$OUT/Mods" wake-of-gods <<'PY'
import json, os, re, shutil, sys

root, ours = sys.argv[1], set(sys.argv[2:])

def load(path):
    text = open(path, encoding='utf-8-sig').read()
    try:
        return json.loads(text)
    except ValueError:
        return json.loads(re.sub(r',(\s*[}\]])', r'\1', text))

# Mod id = the folder names below the top mod, joined by dots, lower case; the "Mods"/"mods" folders are not part
# of it. Every mod folder currently staged (ours and any other tool's) counts as "available" for dependency
# checks, but only ours are ever removed: a shared directory must not have one fetch script prune another's mods.
mods = {}
for top in sorted(os.listdir(root)):
    for dirpath, _, files in os.walk(os.path.join(root, top)):
        if 'mod.json' not in files:
            continue
        parts = [top] + [p for p in os.path.relpath(dirpath, os.path.join(root, top)).split(os.sep) if p.lower() not in ('.', 'mods')]
        mods['.'.join(parts).lower()] = (top, dirpath, [d.lower() for d in load(os.path.join(dirpath, 'mod.json')).get('depends', [])])

removed = []
changed = True
while changed:
    changed = False
    for mod_id, (top, path, depends) in list(mods.items()):
        if mod_id not in mods or top not in ours:
            continue
        if any(d != 'vcmi' and not d.startswith('vcmi.') and d not in mods for d in depends):
            shutil.rmtree(path)
            for other_id, (other_top, other_path, _) in list(mods.items()):
                if other_path == path or other_path.startswith(path + os.sep):
                    del mods[other_id]
                    removed.append(other_id)
            changed = True

kept = sum(1 for top, _, _ in mods.values() if top in ours)
print('kept %d mods, removed %d with unmet dependencies:' % (kept, len(removed)))
for mod_id in removed:
    print('  ' + mod_id)
PY

echo
du -sh "$OUT/Mods/wake-of-gods"
echo "Staged in $OUT/Mods. Copy its contents to /media/fat/vcmi/data/Mods on the MiSTer, or run scripts/deploy.sh mods."
