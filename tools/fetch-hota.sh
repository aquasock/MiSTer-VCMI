#!/usr/bin/env bash
# Fetches the Horn of the Abyss (HotA) mod for VCMI 1.7 and stages it as a drop-in mods directory, without the VCMI launcher.
# Usage: tools/fetch-hota.sh
#   Output: work/hota-mods/Mods/{hota,vcmi-extras}. Copy the contents of that Mods directory into /media/fat/vcmi/data/Mods
#   on the MiSTer (or run `scripts/deploy.sh mods`). Nothing is committed or bundled: HotA is CC BY-SA 4.0 and about 0.5 GB.
# The downloads come from the official VCMI mod repository listing (vcmi-mods-repository, vcmi-1.7.json) and are checked against
# the SHA-256 sums below; if upstream re-releases a mod the check fails, so update the pins after looking at the new release.
# vcmi-extras is needed because a HotA submod depends on vcmi-extras.adventuremap. Submods whose dependencies are not installed
# (the translation button and voice packs) are removed, since VCMI would only mark them broken.
set -euo pipefail
. "$(dirname "${BASH_SOURCE[0]}")/../scripts/env.sh"

HOTA_URL=https://github.com/vcmi-mods/horn-of-the-abyss/releases/download/1.7/horn-of-the-abyss-vcmi-1.7.zip
HOTA_SHA256=e70e6d9fc3ef9541caeda9353d93ea54d47ebb34ae1d403fec37f1dc7522afd2
EXTRAS_URL=https://github.com/vcmi-mods/vcmi-extras/releases/download/1.7/vcmi-extras-vcmi-1.7.zip
EXTRAS_SHA256=c6b253395d0dad3e3eb81aa2a070619ba266a4283096d98b2b962f07119a5c22

OUT="$WORK/hota-mods"
STAGE="$WORK/hota-stage"

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

install_mod() {	# zip id -> $OUT/Mods/<id>
	rm -rf "$STAGE"
	mkdir -p "$STAGE"
	unzip -q "$1" -d "$STAGE"
	mkdir -p "$OUT/Mods"
	rm -rf "$OUT/Mods/$2"
	mv "$STAGE"/* "$OUT/Mods/$2"
	rm -rf "$OUT/Mods/$2/.github" "$OUT/Mods/$2/screenshots" "$STAGE"
}

fetch "$HOTA_URL" "$HOTA_SHA256"
fetch "$EXTRAS_URL" "$EXTRAS_SHA256"

rm -rf "$OUT"
install_mod "$DL/$(basename "$HOTA_URL")" hota
install_mod "$DL/$(basename "$EXTRAS_URL")" vcmi-extras

python3 - "$OUT/Mods" <<'PY'
import json, os, re, shutil, sys

root = sys.argv[1]

def load(path):
    text = open(path, encoding='utf-8-sig').read()
    try:
        return json.loads(text)
    except ValueError:
        return json.loads(re.sub(r',(\s*[}\]])', r'\1', text))

# Mod id = the folder names below the top mod, joined by dots, lower case; the "mods" folders are not part of it.
mods = {}
for top in sorted(os.listdir(root)):
    for dirpath, _, files in os.walk(os.path.join(root, top)):
        if 'mod.json' not in files:
            continue
        parts = [top] + [p for p in os.path.relpath(dirpath, os.path.join(root, top)).split(os.sep) if p not in ('.', 'mods', 'Mods')]
        mods['.'.join(parts).lower()] = (dirpath, [d.lower() for d in load(os.path.join(dirpath, 'mod.json')).get('depends', [])])

removed = []
changed = True
while changed:
    changed = False
    for mod_id, (path, depends) in list(mods.items()):
        if mod_id not in mods:      # already removed together with its parent folder
            continue
        if any(d != 'vcmi' and not d.startswith('vcmi.') and d not in mods for d in depends):
            shutil.rmtree(path)
            for other_id, (other_path, _) in list(mods.items()):
                if other_path == path or other_path.startswith(path + os.sep):
                    del mods[other_id]
                    removed.append(other_id)
            changed = True

print('kept %d mods, removed %d with unmet dependencies:' % (len(mods), len(removed)))
for mod_id in removed:
    print('  ' + mod_id)
PY

echo
du -sh "$OUT/Mods"/* | sed 's|	.*/hota-mods/|	|'
echo "Staged in $OUT/Mods. Copy its contents to /media/fat/vcmi/data/Mods on the MiSTer, or run scripts/deploy.sh mods."
