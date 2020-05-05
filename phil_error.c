/* Copyright Phillip Potter, 2020 - MIT License */
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

/* These are just a shared set of error functions to be used by both server and client */
static void philErrorCore(const char *str, va_list args)
{
	/* Call vfprintf etc. */
	vfprintf(stderr, str, args);
	fprintf(stderr, ": %s\n", strerror(errno));
}

void philErrorMsg(const char *str, ...)
{
	/* Variables */
	va_list list;

	/* Process arguments */
	va_start(list, str);
	philErrorCore(str, list);
	va_end(list);

	/* Reset errno */
	errno = 0;
}

void philError(const char *str, ...)
{
	/* Variables */
	va_list list;

	/* Process arguments */
	va_start(list, str);
	philErrorCore(str, list);
	va_end(list);

	/* Exit with error */
	exit(EXIT_FAILURE);
}
