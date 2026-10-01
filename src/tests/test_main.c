#include "test_locations.h"
#include <stdio.h>

int global_tests_run = 0;
int global_tests_failed = 0;

int main(void) {
    test_locations_run_all();
    
    printf("\n%d/%d passed\n", 
        global_tests_run - global_tests_failed, global_tests_run);
    
    return (global_tests_failed > 0) ? 1 : 0;
}