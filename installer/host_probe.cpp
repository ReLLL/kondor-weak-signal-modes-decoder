// Copyright 2026 ReLLL. SPDX-License-Identifier: GPL-3.0-or-later
// Run in a short-lived process: loading host globals must never affect the installer.
#include <dlfcn.h>
#include <mach-o/fat.h>
#include <mach-o/loader.h>
#include <libkern/OSByteOrder.h>
#include <sys/resource.h>
#include <unistd.h>
#include <cstdint>
#include <cstdio>
#include <fstream>

static bool arm64Executable(const char* path) {
    std::ifstream file(path, std::ios::binary);
    uint32_t magic = 0, cpu = 0;
    file.read(reinterpret_cast<char*>(&magic), 4);
    file.read(reinterpret_cast<char*>(&cpu), 4);
    if (!file) return false;
    if (magic == MH_MAGIC_64) return cpu == CPU_TYPE_ARM64;
    if (magic != FAT_CIGAM && magic != FAT_CIGAM_64) return false;
    auto count = OSSwapBigToHostInt32(cpu);
    if (!count || count > 64) return false;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t type = 0;
        file.read(reinterpret_cast<char*>(&type), 4);
        file.seekg(magic == FAT_CIGAM ? 16 : 28, std::ios::cur);
        if (!file) return false;
        if (OSSwapBigToHostInt32(type) == CPU_TYPE_ARM64) return true;
    }
    return false;
}

int main(int argc, char** argv) {
    rlimit core{0, 0}; setrlimit(RLIMIT_CORE, &core);
    rlimit output{65536, 65536}; setrlimit(RLIMIT_FSIZE, &output);
    if (argc != 4) { dprintf(2, "Expected host executable, host core and plugin.\n"); _exit(2); }
    if (!arm64Executable(argv[1])) {
        dprintf(2, "The selected host needs an Apple Silicon (arm64) executable. Install the arm64 or Universal host package.\n"); _exit(3);
    }
    // Core is loaded explicitly first so the tested plugin cannot select another host.
    void* host = dlopen(argv[2], RTLD_NOW | RTLD_GLOBAL);
    if (!host) { dprintf(2, "Host runtime: %s\n", dlerror()); _exit(4); }
    void* plugin = dlopen(argv[3], RTLD_NOW | RTLD_LOCAL);
    if (!plugin) { dprintf(2, "Plugin runtime: %s\n", dlerror()); _exit(5); }
    for (auto symbol : {"_INFO_", "_INIT_", "_CREATE_INSTANCE_", "_DELETE_INSTANCE_", "_END_"}) {
        if (!dlsym(plugin, symbol)) { dprintf(2, "Missing plugin entry point: %s\n", symbol); _exit(6); }
    }
    auto check = reinterpret_cast<bool(*)()>(dlsym(plugin, "_DX_CHECK_HOST_"));
    if (!check || !check()) { dprintf(2, "Host ImGui ABI does not match this plugin build. A matching plugin build is required.\n"); _exit(7); }
    dprintf(1, "Host architecture, dependency loading, symbols and ImGui layout passed.\n");
    // SDR++'s global waterfall destructor requires an initialized GL context.
    // No module instance, radio, profile or reception writer was started here.
    _exit(0);
}
