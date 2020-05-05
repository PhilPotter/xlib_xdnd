/* Copyright Phillip Potter, 2020 - MIT License
 * Header file for square-related routines and structure */
#ifndef SQUARE_STATE
#define SQUARE_STATE

typedef enum { RedSquare = 0, BlueSquare = 1 } SquareColour;

// Square structure
typedef struct {
	int x;
	int y;
	int mouse_x;
	int mouse_y;
	int size;
	bool visible;
	bool selected;
	SquareColour colour;
} Square;

const char *saveSquareState(Square *square);
void restoreSquareState(const char *pathStr, Square *square);

#endif
