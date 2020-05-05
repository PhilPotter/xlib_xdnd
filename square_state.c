/* Copyright Phillip Potter, 2020 - MIT License
 * This contains the routine to save and load state for the square from a temporary file */
#include <stdio.h>
#include <stdbool.h>
#include "square_state.h"
#include "phil_error.h"

#define SQUARE_FILE_PATH "/tmp/square.state"

// This function saves the square state to a temporary file and returns its pathname 
const char *saveSquareState(Square *square)
{
	// Create/truncate the file
	FILE *squareFile = fopen(SQUARE_FILE_PATH, "wb");
	if (!squareFile)
		philError("fopen");

	// Store state
	if (fwrite(&square->colour, sizeof(SquareColour), 1, squareFile) != 1)
		philError("fwrite");

	// Close file
	if (fclose(squareFile) != 0)
		philError("fclose");

	return SQUARE_FILE_PATH;
}

// This function restores the square state from the supplied path
void restoreSquareState(const char *pathStr, Square *square)
{
	// Open the file
	FILE *squareFile = fopen(pathStr, "rb");
	if (!squareFile)
		philError("fopen");

	// Restore state
	if (fread(&square->colour, sizeof(SquareColour), 1, squareFile) != 1)
		philError("fread");

	// Close file
	if (fclose(squareFile) != 0)
		philError("fclose");
}
