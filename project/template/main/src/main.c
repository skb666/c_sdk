#include <stdio.h>

#include "global_build_info_time.h"
#include "global_build_info_version.h"
#include "global_config.h"
#include "test.h"

// #include "lib2_private.h"  // We can't include lib2_private.h for it's compoent2's private include dir

int main() {
    printf("hello\n");
    test();

    return 0;
}
