#!/usr/bin/env python3
"""Convert a PNG into a compiled LVGL header mark and register it."""

from __future__ import annotations

import argparse
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GENERATED = ROOT / "src" / "ui" / "generated"
REGISTRY = ROOT / "src" / "ui" / "compiled_marks.generated.h"


def identifier(value: str) -> str:
    clean = re.sub(r"[^a-zA-Z0-9_]+", "_", value.strip()).strip("_").lower()
    if not clean:
        raise SystemExit("mark ID must contain a letter or number")
    if clean[0].isdigit():
        clean = "mark_" + clean
    return clean


def write_asset(source: Path, mark_id: str, width: int, height: int) -> str:
    try:
        from PIL import Image
    except ImportError as exc:
        raise SystemExit("Pillow is required: python -m pip install Pillow") from exc
    symbol = "command_deck_mark_" + identifier(mark_id)
    image = Image.open(source).convert("RGBA")
    image.thumbnail((width, height), Image.Resampling.LANCZOS)
    canvas = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    canvas.alpha_composite(image, ((width - image.width) // 2, (height - image.height) // 2))
    data: list[int] = []
    for red, green, blue, alpha in canvas.getdata():
        data.extend((blue, green, red, alpha))

    GENERATED.mkdir(parents=True, exist_ok=True)
    target = GENERATED / f"{symbol}.cpp"
    rows = []
    for start in range(0, len(data), 16):
        rows.append("    " + ", ".join(f"0x{v:02x}" for v in data[start:start + 16]) + ",")
    target.write_text(
        '#include <lvgl.h>\n\n'
        f'static const uint8_t {symbol}_map[] = {{\n' + "\n".join(rows) + "\n};\n\n"
        f'extern const lv_image_dsc_t {symbol} = {{\n'
        '    .header.cf = LV_COLOR_FORMAT_ARGB8888,\n'
        '    .header.magic = LV_IMAGE_HEADER_MAGIC,\n'
        f'    .header.w = {width},\n'
        f'    .header.h = {height},\n'
        f'    .data_size = sizeof({symbol}_map),\n'
        f'    .data = {symbol}_map,\n'
        '};\n',
        encoding="ascii",
    )
    return symbol


def rebuild_registry() -> None:
    entries = []
    for path in sorted(GENERATED.glob("command_deck_mark_*.cpp")):
        symbol = path.stem
        mark_id = symbol.removeprefix("command_deck_mark_").replace("_", "-")
        entries.append((mark_id, symbol))
    declarations = "\n".join(f"extern const lv_image_dsc_t {symbol};" for _, symbol in entries)
    macro = "#define COMMAND_DECK_CUSTOM_MARKS(X)"
    if entries:
        macro += " \\\n" + " \\\n".join(f'    X("{mark_id}", {symbol})' for mark_id, symbol in entries)
    REGISTRY.write_text(
        "#pragma once\n#include <lvgl.h>\n\n" + declarations + "\n\n" + macro + "\n",
        encoding="ascii",
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path, help="transparent PNG source")
    parser.add_argument("id", help="stable profile mark ID")
    parser.add_argument("--width", type=int, default=32)
    parser.add_argument("--height", type=int, default=32)
    args = parser.parse_args()
    if not args.source.is_file():
        raise SystemExit(f"source does not exist: {args.source}")
    symbol = write_asset(args.source, args.id, args.width, args.height)
    rebuild_registry()
    print(f"registered {args.id!r} as {symbol}; rebuild command-deck")


if __name__ == "__main__":
    main()
