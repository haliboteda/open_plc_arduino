/* The board's FatFs configuration, plus f_mkfs so the test can format its RAM
 * disk. Found before OpenPLC_SD's ffconf.h, which needs the STM32 headers. */
#include "ffconf_default_80286.h"
#undef FF_USE_MKFS
#define FF_USE_MKFS 1
