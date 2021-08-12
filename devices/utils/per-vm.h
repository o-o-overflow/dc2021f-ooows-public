#pragma once
#define MAX_VMNAME_LEN 32
#include <stdbool.h>

int createShm(char *basename, bool is_rdonly);
char *concatName(char *base);
