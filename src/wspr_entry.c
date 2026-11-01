/* Copyright 2026 ReLLL. SPDX-License-Identifier: GPL-3.0-or-later */
#include <string.h>
#include <unistd.h>
int dx_wspr_main(int argc, char** argv);
int main(int argc, char** argv) {
    if (argc == 2 && strcmp(argv[1], "--check-runtime") == 0) _exit(0);
    return dx_wspr_main(argc, argv);
}
