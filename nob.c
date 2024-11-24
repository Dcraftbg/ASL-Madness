#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "nob.h"

int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);
    Cmd cmd = {0};
    cmd_append(&cmd, "cc", "-Wall", "-Wextra", "-o", "main", "./src/main.c", "-I", ".");
    if (!cmd_run_sync_and_reset(&cmd)) return 1;
    cmd_append(&cmd, "./main", "examples/simple.aml");
    if (!cmd_run_sync_and_reset(&cmd)) return 1;
}
