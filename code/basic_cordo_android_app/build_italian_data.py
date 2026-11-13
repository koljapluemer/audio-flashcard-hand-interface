#!/usr/bin/env python3
"""Bundle data/italian-english-sentences/ into the app's www/ folder.

Reads every <LESSON>.tsv (columns: number, English, Italian), writes
www/learning_data/it/sentences.json and copies the matching
<LESSON>_<NNN>_{it,en}.mp3 files next to it. Row 001 of each lesson is the
lesson title, not a sentence, so it is skipped. Rows without both audio files
are skipped with a warning.

Run from anywhere: python3 code/basic_cordo_android_app/build_italian_data.py
"""
import json
import shutil
import sys
from pathlib import Path

APP = Path(__file__).resolve().parent
SRC = APP.parent.parent / "data" / "italian-english-sentences"
OUT = APP / "www" / "learning_data" / "it"


def main():
    if not SRC.is_dir():
        sys.exit(f"missing source folder: {SRC}")
    if OUT.exists():
        shutil.rmtree(OUT)
    (OUT / "audio").mkdir(parents=True)

    sentences = []
    for tsv in sorted(SRC.glob("*.tsv")):
        lesson = tsv.stem
        for line in tsv.read_text(encoding="utf-8").splitlines():
            if not line.strip():
                continue
            num, en, it = (c.strip() for c in line.split("\t"))
            if num == "001":
                continue  # lesson title
            sid = f"{lesson}_{num}"
            audio = {lang: SRC / f"{sid}_{lang}.mp3" for lang in ("it", "en")}
            missing = [p.name for p in audio.values() if not p.exists()]
            if missing:
                print(f"skip {sid}: missing {', '.join(missing)}", file=sys.stderr)
                continue
            for p in audio.values():
                shutil.copy2(p, OUT / "audio" / p.name)
            sentences.append({"id": sid, "it": it, "en": en})

    (OUT / "sentences.json").write_text(
        json.dumps(sentences, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    print(f"{len(sentences)} sentences -> {OUT.relative_to(APP.parent.parent)}")


if __name__ == "__main__":
    main()
