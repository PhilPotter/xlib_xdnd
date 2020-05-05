/* Copyright Phillip Potter, 2020 - MIT License
 * This program demonstrates basic usage of the XDND drag and drop protocol via Xlib */
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include "phil_error.h"
#include "spawn_window.h"

/* Entry point */
int main(void)
{
	// Variables
	pid_t procId;

	// Fork
	switch (procId = fork()) {
	case -1:
		// Error forking
		philError("fork");
	case 0:
		// Child process
		spawnWindow(procId);
	default:
		// Parent process
		spawnWindow(procId);
	}
}
