Import("env")

from pathlib import Path


def patch_file(path: Path) -> bool:
    if not path.exists():
        print(f"[m5core2-i2c] skip missing file: {path}")
        return False

    original = path.read_text(encoding="utf-8")
    patched = original.replace("Wire1", "Wire")

    if patched == original:
        print(f"[m5core2-i2c] no changes needed: {path.name}")
        return False

    path.write_text(patched, encoding="utf-8")
    print(f"[m5core2-i2c] patched: {path.name}")
    return True


def apply_patches():
    project_dir = Path(env["PROJECT_DIR"])
    libdeps_dir = Path(env["PROJECT_LIBDEPS_DIR"])
    pioenv = env["PIOENV"]

    m5_dir = libdeps_dir / pioenv / "M5Core2" / "src"
    files = [
        m5_dir / "M5Touch.cpp",
        m5_dir / "RTC.cpp",
        m5_dir / "AXP.cpp",
        m5_dir / "AXP192.cpp",
        m5_dir / "utility" / "MPU6886.cpp",
    ]

    patched_any = False
    for file_path in files:
        patched_any = patch_file(file_path) or patched_any

    if not patched_any:
        print("[m5core2-i2c] no file updates were required")


apply_patches()
