#!/usr/bin/env python3
"""
Generate ProtoNerdFontData.h (16px) and ProtoNerdValueFontData.h (24px)
from the Martian Mono Nerd Font.

Both output files use the 'proto_nerd_font' namespace so zero C++ changes
are needed in the rendering code (ProtoNerdFont.cpp, ProtoNerdValueFont.cpp).

Usage:
    python gen_martian_mono_fonts.py
"""

import io
import os
import sys
import zipfile
import urllib.request
import subprocess

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

FONT_URL = (
    "https://github.com/ryanoasis/nerd-fonts/releases/latest/download/MartianMono.zip"
)
FONT_FILENAME_CANDIDATES = [
    "MartianMonoNerdFont-Regular.ttf",
    "MartianMono-Regular.ttf",
    "MartianMonoNFM-Regular.ttf",
]
FONT_LOCAL_PATH = os.path.join(SCRIPT_DIR, "MartianMonoNerdFont-Regular.ttf")

FIRST_CHAR = 32   # space
LAST_CHAR  = 126  # ~


# ---------------------------------------------------------------------------
# Dependency bootstrap
# ---------------------------------------------------------------------------

def _pip_install(*packages):
    subprocess.check_call(
        [sys.executable, "-m", "pip", "install", "--quiet"] + list(packages)
    )


def ensure_deps():
    missing = []
    try:
        import freetype  # noqa: F401
    except ImportError:
        missing.append("freetype-py")
    if missing:
        print(f"Installing: {missing}")
        _pip_install(*missing)


# ---------------------------------------------------------------------------
# Font download
# ---------------------------------------------------------------------------

def download_font() -> str:
    if os.path.exists(FONT_LOCAL_PATH):
        print(f"Font cache: {FONT_LOCAL_PATH}")
        return FONT_LOCAL_PATH

    print(f"Downloading MartianMono.zip from GitHub …")
    with urllib.request.urlopen(FONT_URL) as resp:
        data = resp.read()

    with zipfile.ZipFile(io.BytesIO(data)) as zf:
        names = zf.namelist()
        print(f"  {len(names)} files in zip")

        target_member = None
        for candidate in FONT_FILENAME_CANDIDATES:
            for name in names:
                if name.endswith(candidate) or os.path.basename(name) == candidate:
                    target_member = name
                    break
            if target_member:
                break

        if target_member is None:
            # Fallback: pick any Regular TTF
            for name in names:
                bn = os.path.basename(name)
                if bn.endswith(".ttf") and "Regular" in bn and "NFM" not in bn:
                    target_member = name
                    break

        if target_member is None:
            raise RuntimeError(
                "Could not find a Regular TTF in the zip.\n"
                f"Available files:\n" + "\n".join(f"  {n}" for n in names)
            )

        print(f"  Extracting: {target_member}")
        font_bytes = zf.read(target_member)

    with open(FONT_LOCAL_PATH, "wb") as f:
        f.write(font_bytes)

    print(f"  Saved to: {FONT_LOCAL_PATH}  ({len(font_bytes):,} bytes)")
    return FONT_LOCAL_PATH


# ---------------------------------------------------------------------------
# Bitmap rendering via FreeType
# ---------------------------------------------------------------------------

def render_font(font_path: str, pixel_height: int):
    """
    Render ASCII 32-126 from *font_path* at *pixel_height*.

    Returns:
        cell_w        – advance width in pixels (monospace)
        cell_h        – total cell height = ascent + descent
        bytes_per_row – ceil(cell_w / 8)
        glyph_bytes   – packed 1-bit-per-pixel bitmap for all glyphs
        ascent        – pixels above baseline
        descent       – pixels below baseline (positive value)
    """
    import freetype  # type: ignore

    face = freetype.Face(font_path)
    face.set_pixel_sizes(0, pixel_height)

    # Measure advance from a representative character ('0').
    face.load_char(
        "0",
        freetype.FT_LOAD_RENDER | freetype.FT_LOAD_TARGET_MONO,
    )
    cell_w    = face.glyph.advance.x >> 6
    ascent    = face.size.ascender >> 6
    descent   = -(face.size.descender >> 6)   # descender is negative in FreeType
    cell_h    = ascent + descent

    bytes_per_row   = (cell_w + 7) // 8
    bytes_per_glyph = bytes_per_row * cell_h

    glyph_bytes = bytearray()

    for code in range(FIRST_CHAR, LAST_CHAR + 1):
        ch = chr(code)
        face.load_char(
            ch,
            freetype.FT_LOAD_RENDER | freetype.FT_LOAD_TARGET_MONO,
        )
        g = face.glyph
        bm = g.bitmap

        cell = bytearray(bytes_per_glyph)

        for row in range(bm.rows):
            y_in_cell = ascent - g.bitmap_top + row
            if y_in_cell < 0 or y_in_cell >= cell_h:
                continue
            for col in range(bm.width):
                x_in_cell = g.bitmap_left + col
                if x_in_cell < 0 or x_in_cell >= cell_w:
                    continue
                # FreeType MONO bitmap: MSB-first, 1-bit/pixel
                src_byte_idx = row * bm.pitch + (col >> 3)
                src_bit      = 7 - (col & 7)
                if (
                    src_byte_idx < len(bm.buffer)
                    and (bm.buffer[src_byte_idx] >> src_bit) & 1
                ):
                    dst_byte_idx = y_in_cell * bytes_per_row + (x_in_cell >> 3)
                    dst_bit      = 7 - (x_in_cell & 7)
                    cell[dst_byte_idx] |= 1 << dst_bit

        glyph_bytes.extend(cell)

    return cell_w, cell_h, bytes_per_row, bytes(glyph_bytes), ascent, descent


# ---------------------------------------------------------------------------
# Header file writer
# ---------------------------------------------------------------------------

def write_header(
    out_path: str,
    namespace: str,
    cell_w: int,
    cell_h: int,
    bytes_per_row: int,
    ascent: int,
    descent: int,
    glyph_bytes: bytes,
):
    bytes_per_glyph = bytes_per_row * cell_h
    num_glyphs      = LAST_CHAR - FIRST_CHAR + 1

    lines = [
        "#pragma once",
        "#include <stdint.h>",
        "",
        f"namespace {namespace} {{",
        f"static constexpr uint8_t  kFirst        = {FIRST_CHAR};",
        f"static constexpr uint8_t  kLast         = {LAST_CHAR};",
        f"static constexpr uint8_t  kCellWidth    = {cell_w};",
        f"static constexpr uint8_t  kCellHeight   = {cell_h};",
        f"static constexpr uint8_t  kBytesPerRow  = {bytes_per_row};",
        f"static constexpr uint16_t kBytesPerGlyph= {bytes_per_glyph};",
        f"static constexpr uint8_t  kAscent       = {ascent};",
        f"static constexpr uint8_t  kDescent      = {descent};",
        f"static constexpr uint8_t  kLineHeight   = {cell_h};",
        "",
        "static const uint8_t kBitmap[] = {",
    ]

    # Emit one row (bytes_per_row bytes) per source line for readability.
    for glyph_idx in range(num_glyphs):
        glyph_off = glyph_idx * bytes_per_glyph
        # Comment showing which character this glyph is.
        ch = chr(FIRST_CHAR + glyph_idx)
        display_ch = ch if ch.isprintable() and ch != " " else f"U+{FIRST_CHAR + glyph_idx:04X}"
        lines.append(f"  // '{display_ch}'")
        for row in range(cell_h):
            row_off   = glyph_off + row * bytes_per_row
            row_bytes = glyph_bytes[row_off : row_off + bytes_per_row]
            hex_str   = ", ".join(f"0x{b:02X}" for b in row_bytes)
            lines.append(f"  {hex_str},")

    lines.append("};")
    lines.append("")
    lines.append(f"}}  // namespace {namespace}")
    lines.append("")

    with open(out_path, "w", newline="\n") as f:
        f.write("\n".join(lines))

    total_bytes = len(glyph_bytes)
    print(
        f"  Written: {os.path.basename(out_path)}  "
        f"({num_glyphs} glyphs, {cell_w}x{cell_h}px, "
        f"{bytes_per_glyph}B/glyph, {total_bytes}B total)"
    )


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    ensure_deps()

    font_path = download_font()

    # ---- 16px label/body font (ProtoNerdFontData.h) ----
    print("\nRendering body font at 16px …")
    cw, ch, bpr, gdata, asc, dsc = render_font(font_path, 16)
    out_body = os.path.join(SCRIPT_DIR, "ProtoNerdFontData.h")
    write_header(out_body, "proto_nerd_font", cw, ch, bpr, asc, dsc, gdata)

    # ---- 24px value/tile font (ProtoNerdValueFontData.h) ----
    print("\nRendering value font at 24px …")
    cw, ch, bpr, gdata, asc, dsc = render_font(font_path, 24)
    out_value = os.path.join(SCRIPT_DIR, "ProtoNerdValueFontData.h")
    write_header(out_value, "proto_nerd_font", cw, ch, bpr, asc, dsc, gdata)

    print("\nDone – rebuild the firmware to use the new Martian Mono fonts.")


if __name__ == "__main__":
    main()
