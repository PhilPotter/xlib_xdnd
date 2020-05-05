/* Copyright Phillip Potter, 2020 - MIT License
 * Header file for error functions */
#ifndef PHIL_ERROR
#define PHIL_ERROR

#include <stdarg.h>

/* Declare error functions */
void philErrorMsg(const char *str, ...);
void philError(const char *str, ...);

#endif
