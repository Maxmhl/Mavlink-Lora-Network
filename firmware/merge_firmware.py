"""PlatformIO post-build script: create a single merged flash image.

The merged image contains bootloader + partition table + boot_app0 + app and
can be flashed in one step at offset 0x0 (this is what the Windows config
tool expects). Output: .pio/build/<env>/firmware-merged.bin
"""

Import("env")  # noqa: F821


def merge_bin(source, target, env):
    build_dir = env.subst("$BUILD_DIR")
    prog = env.subst("$PROGNAME")
    merged = "%s/%s-merged.bin" % (build_dir, prog)

    cmd = [
        '"%s"' % env.subst("$PYTHONEXE"),
        '"%s"' % env.subst("$OBJCOPY"),  # esptool.py in espressif32 platform
        "--chip",
        env.BoardConfig().get("build.mcu"),
        "merge_bin",
        "-o",
        '"%s"' % merged,
    ]
    # bootloader / partition table / boot_app0 images with their offsets
    for offset, image in env.get("FLASH_EXTRA_IMAGES", []):
        cmd += [env.subst(offset), '"%s"' % env.subst(image)]
    # the application itself
    cmd += [env.subst("$ESP32_APP_OFFSET"), '"%s/%s.bin"' % (build_dir, prog)]

    env.Execute(" ".join(cmd))


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_bin)
