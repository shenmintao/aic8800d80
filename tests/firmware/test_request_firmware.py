#!/usr/bin/env python3
"""Exercise the real firmware request/load functions with mocked kernel services."""
from pathlib import Path
import os
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


def function(source, signature):
    start = source.index(signature)
    brace = source.index("{", start)
    end, depth = brace + 1, 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


PRELUDE = r'''
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CONFIG_USE_FW_REQUEST 1
#define printk(...) ((void)0)
#define AICWFDBG(...) ((void)0)
typedef uint32_t u32;
typedef int MD5_CTX;
static void MD5Init(MD5_CTX *m) {}
static void MD5Update(MD5_CTX *m, unsigned char *b, int size) {}
static void MD5Final(MD5_CTX *m, unsigned char *b) {}
struct firmware { size_t size; const unsigned char *data; };
struct aic_usb_dev { int chipid; };
struct aicwf_bus { struct { struct aic_usb_dev *usb; } bus_priv; };
struct device { void *data; };
struct rwnx_hw { struct device *dev; };
static struct aic_usb_dev usb;
static struct aicwf_bus bus = { .bus_priv.usb = &usb };
static struct device dev = { .data = &bus };
static struct device *expected_device;
static struct rwnx_hw hw;
static const unsigned char bytes[] = { 1, 3, 5, 7, 9 };
static struct firmware fw = { sizeof(bytes), bytes };
static const char *names[2];
static int errors[2], requests, releases, allocation_fail;
static void *dev_get_drvdata(struct device *d) { return d->data; }
static int request_firmware(const struct firmware **out, const char *name,
                            struct device *d) {
    assert(requests < 2 && names[requests]);
    assert(!strcmp(name, names[requests]));
    assert(d == expected_device);
    int err = errors[requests++];
    *out = err ? NULL : &fw;
    return err;
}
static void release_firmware(const struct firmware *f) {
    if (f) { assert(f == &fw); releases++; }
}
static void *vmalloc(size_t size) { return allocation_fail ? NULL : malloc(size); }
static void setup(int chip, const char *name) {
    usb.chipid = chip; bus.bus_priv.usb = &usb; dev.data = &bus;
    hw.dev = expected_device = &dev;
    names[0] = name; names[1] = NULL;
    errors[0] = errors[1] = requests = releases = allocation_fail = 0;
    fw.size = sizeof(bytes);
}
'''

CHECKS = r'''
static void success(void) {
    u32 *out = (void *)1;
    assert(load(&out, "fw.bin") == sizeof(bytes));
    assert(out && !memcmp(out, bytes, sizeof(bytes)));
    assert(releases == 1);
    free(out);
}
int main(void) {
    for (unsigned int i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        char path[256];
        snprintf(path, sizeof(path), "%s/fw.bin", cases[i].dir);
#if defined(CONFIG_PLATFORM_UBUNTU) && defined(AICWF_USB_SUPPORT)
        setup(cases[i].chip, path);
#else
        setup(cases[i].chip, "fw.bin");
#endif
        success(); assert(requests == 1);
    }
    setup(-1, "fw.bin"); success();
    setup(cases[0].chip, "fw.bin"); hw.dev = expected_device = NULL; success();
    setup(cases[0].chip, "fw.bin"); dev.data = NULL; success();
    setup(cases[0].chip, "fw.bin"); bus.bus_priv.usb = NULL; success();
#if defined(CONFIG_PLATFORM_UBUNTU) && defined(AICWF_USB_SUPPORT)
    char path[256];
    snprintf(path, sizeof(path), "%s/fw.bin", cases[0].dir);
    setup(cases[0].chip, path);
    errors[0] = -ENOENT; names[1] = "fw.bin";
    success(); assert(requests == 2);
    setup(cases[0].chip, path);
    errors[0] = -ENOENT; names[1] = "fw.bin"; errors[1] = -ENOENT;
    u32 *out = (void *)1;
    assert(load(&out, "fw.bin") == -ENOENT && !out && requests == 2);
    setup(cases[0].chip, path); errors[0] = -EACCES; out = (void *)1;
    assert(load(&out, "fw.bin") == -EACCES && !out && requests == 1);
    setup(cases[0].chip, path); errors[0] = -ENOMEM; out = (void *)1;
    assert(load(&out, "fw.bin") == -ENOMEM && !out && requests == 1);
    char long_name[300]; memset(long_name, 'a', sizeof(long_name));
    long_name[sizeof(long_name) - 1] = 0;
    setup(cases[0].chip, NULL); out = (void *)1;
    assert(load(&out, long_name) == -ENAMETOOLONG && !out && !requests);
#endif
    u32 *buffer = (void *)1;
    setup(-1, "fw.bin"); allocation_fail = 1;
    assert(load(&buffer, "fw.bin") == -ENOMEM && !buffer && releases == 1);
    setup(-1, "fw.bin"); fw.size = 0; buffer = (void *)1;
    assert(load(&buffer, "fw.bin") == -EINVAL && !buffer && releases == 1);
    setup(-1, "fw.bin"); fw.size = (size_t)INT_MAX + 1; buffer = (void *)1;
    assert(load(&buffer, "fw.bin") == -EINVAL && !buffer && releases == 1);
    return 0;
}
'''


class RequestFirmwareTests(unittest.TestCase):
    def test_paths_device_context_and_failures(self):
        variants = {
            "aic_load_fw": ("aicbluetooth.c", "aic_load_firmware", {
                "aic8800": ["AIC8800", "AIC8801"],
                "aic8800DC": ["AIC8800DC", "AIC8800DW"],
                "aic8800D80": ["AIC8800D80", "AIC8800D81"],
                "aic8800D80X2": ["AIC8800D80X2", "AIC8800D81X2",
                                  "AIC8800D89X2", "AIC8800D40X2"],
            }),
            "aic8800_fdrv": ("rwnx_platform.c", "rwnx_load_firmware", {
                "aic8800": ["AIC8801"],
                "aic8800DC": ["AIC8800DC", "AIC8800DW"],
                "aic8800D80": ["AIC8800D81"],
                "aic8800D80X2": ["AIC8800D81X2", "AIC8800D89X2"],
                "aic8800D80N": ["AIC8800D80N"],
                "aic8800DLN": ["AIC8800DLN"],
            }),
        }
        for module, (filename, loader, mapping) in variants.items():
            folder = ROOT / "drivers/aic8800" / module
            source = (folder / filename).read_text()
            enums = re.search(r"enum AICWF_IC\s*\{.*?\};",
                              (folder / "aicwf_usb.h").read_text(), re.S).group()
            code = PRELUDE + enums + function(source, "static int aic_request_firmware(")
            code += function(source, "static int " + loader + "(")
            if module == "aic8800_fdrv":
                code += function(source, "int rwnx_request_firmware_common(")
                call = "rwnx_request_firmware_common(&hw, out, name)"
            else:
                call = "aic_load_firmware(out, name, hw.dev)"
            code += "\nstatic int load(u32 **out, const char *name) { return " + call + "; }\n"
            code += "static const struct { int chip; const char *dir; } cases[] = {\n"
            for directory, chips in mapping.items():
                for chip in chips:
                    code += '{PRODUCT_ID_' + chip + ', "' + directory + '"},\n'
            code += "};\n" + CHECKS
            for defines in ([], ["CONFIG_PLATFORM_UBUNTU", "AICWF_USB_SUPPORT"],
                            ["CONFIG_PLATFORM_UBUNTU"]):
                with self.subTest(module=module, defines=defines):
                    with tempfile.TemporaryDirectory(prefix="aic-fw-request-") as temp:
                        c_file, binary = Path(temp) / "test.c", Path(temp) / "test"
                        c_file.write_text(code)
                        subprocess.run([os.environ.get("CC", "cc"), "-std=gnu11",
                                        *["-D" + d for d in defines], "-o", str(binary),
                                        str(c_file)], check=True)
                        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
