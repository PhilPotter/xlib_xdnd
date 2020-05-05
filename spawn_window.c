/* Copyright Phillip Potter, 2020 - MIT License
 * This spawns the window for each process, and contains the logic for handling events */
#include <sys/types.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <X11/Xlib.h>
#include "spawn_window.h"
#include "square_state.h"
#include "phil_error.h"
#include "xevent_type.h"

#define XDND_PROTOCOL_VERSION 5

// State machine structure
typedef struct {
	bool xdndExchangeStarted;
	bool xdndPositionReceived;
	bool xdndStatusReceived;
	bool xdndStatusSent;
	bool xdndDropReceived;
	Time xdndDropTimestamp;
	Time xdndLastPositionTimestamp;
	bool amISource;
	int p_rootX;
	int p_rootY;
	Window otherWindow;
	Atom proposedAction;
	Atom proposedType;
} XDNDStateMachine;

// Atom definitions
static Atom XdndAware, XA_ATOM, XdndEnter, XdndPosition, XdndActionCopy, XdndLeave, XdndStatus, XdndDrop,
	    XdndSelection, XDND_DATA, XdndTypeList, XdndFinished, WM_PROTOCOLS, WM_DELETE_WINDOW, typesWeAccept[6];

// XDND global state machine
static XDNDStateMachine xdndState;

// This draws the square inside the window, after clearing its contents
static void drawSquare(Display *disp, Window wind, GC gContext, Square *square)
{
	XClearWindow(disp, wind);
	if (square->visible) {
		XFillRectangle(disp, wind, gContext, square->x, square->y,
				square->size, square->size);
	}
	XFlush(disp);
}

// This tells us if the pointer is inside the square, using coordinates relative
// to the host window
static bool isPointerInsideSquare(int x, int y, Square *square)
{
	return x >= square->x && x < square->x + square->size &&
		y >= square->y && y < square->y + square->size;
}

// This somewhat naively calculates what window we are over by drilling down
// to its children and so on using recursion
static Window getWindowPointerIsOver(Display *disp, Window startingWindow,
	int p_rootX, int p_rootY, int originX, int originY)
{
	// Window we are returning
	Window returnWindow = None;

	// Get stacked list of children in stacked order
	Window rootReturn, parentReturn, childReturn, *childList;
	unsigned int numOfChildren;
	if (XQueryTree(disp, startingWindow, &rootReturn, &parentReturn,
		&childList, &numOfChildren) != 0) {
		// Search through children
		for (int i = numOfChildren - 1; i >= 0; --i) {
			// Get window attributes
			XWindowAttributes childAttrs;
			XGetWindowAttributes(disp, childList[i], &childAttrs);

			// Check if cursor is in this window
			if (p_rootX >= originX + childAttrs.x &&
				p_rootX < originX + childAttrs.x + childAttrs.width &&
				p_rootY >= originY + childAttrs.y &&
				p_rootY < originY + childAttrs.y + childAttrs.height) {
				returnWindow = getWindowPointerIsOver(disp, childList[i],
					p_rootX, p_rootY, originX + childAttrs.x, originY + childAttrs.y);
				break;
			}
		}
		XFree(childList);
	}

	// We are are bottom of recursion stack, set correct window to be returned up through each level
	if (returnWindow == None)
		returnWindow = startingWindow;

	return returnWindow;
}

// This checks if the supplied window has the XdndAware property
static int hasCorrectXdndAwareProperty(Display *disp, Window wind) {
	// Try to get property
	int retVal = 0;
	Atom actualType = None;
	int actualFormat;
	unsigned long numOfItems, bytesAfterReturn;
	unsigned char *data = NULL;
	if (XGetWindowProperty(disp, wind, XdndAware, 0, 1024, False, AnyPropertyType,
		&actualType, &actualFormat, &numOfItems, &bytesAfterReturn, &data) == Success) {
		if (actualType != None) {
			// Assume architecture is little endian and just read first byte for
			// XDND protocol version
			if (data[0] <= XDND_PROTOCOL_VERSION) {
				retVal = data[0];
			}

			XFree(data);
		}
	}

	return retVal;
}

// This function prints the contents of ClientMessage events
static void printClientMessage(Display *disp, XClientMessageEvent *message)
{
	// Get atom type as string
	char *messageTypeStr = XGetAtomName(disp, message->message_type);
	printf("Message type: %s\n", messageTypeStr);
	XFree(messageTypeStr);

	// Handle format
	printf("Message word size: %d bits\n", message->format);
	printf("Message ");
	switch (message->format) {
	case 8:
		printf("bytes: ");
		for (int i = 0; i < 20; ++i)
			printf("%d ", message->data.b[i]);
		break;
	case 16:
		printf("16-bit shorts: ");
		for (int i = 0; i < 10; ++i)
			printf("%d ", message->data.s[i]);
		break;
	case 32:
		printf("32-bit longs: ");
		for (int i = 0; i < 5; ++i)
			printf("%d ", message->data.l[i]);
		break;
	}
	printf("\n");
}

// This sends the XdndEnter message which initiates the XDND protocol exchange
static void sendXdndEnter(Display *disp, int xdndVersion, Window source, Window target)
{
	// Only send if we are not already in an exchange
	if (!xdndState.xdndExchangeStarted) {		
		// Declare message struct and populate its values
		XEvent message;
		memset(&message, 0, sizeof(message));
		message.xclient.type = ClientMessage;
		message.xclient.display = disp;
		message.xclient.window = target;
		message.xclient.message_type = XdndEnter;
		message.xclient.format = 32;
		message.xclient.data.l[0] = source;
		message.xclient.data.l[1] = xdndVersion << 24;
		message.xclient.data.l[2] = typesWeAccept[0];
		message.xclient.data.l[3] = None;
		message.xclient.data.l[4] = None;

		// Send it to target window
		if (XSendEvent(disp, target, False, 0, &message) == 0)
			philError("XSendEvent");
	}
}

// This sends the XdndPosition messages, which update the target on the state of the cursor
// and selected action
static void sendXdndPosition(Display *disp, Window source, Window target, int time, int p_rootX, int p_rootY)
{
	if (xdndState.xdndExchangeStarted && xdndState.amISource) {
		// Declare message struct and populate its values
		XEvent message;
		memset(&message, 0, sizeof(message));
		message.xclient.type = ClientMessage;
		message.xclient.display = disp;
		message.xclient.window = target;
		message.xclient.message_type = XdndPosition;
		message.xclient.format = 32;
		message.xclient.data.l[0] = source;
		//message.xclient.data.l[1] reserved
		message.xclient.data.l[2] = p_rootX << 16 | p_rootY;
		message.xclient.data.l[3] = time;
		message.xclient.data.l[4] = XdndActionCopy;

		// Send it to target window
		if (XSendEvent(disp, target, False, 0, &message) == 0)
			philError("XSendEvent");
	}
}

// This is sent by the source when the exchange is abandoned
static void sendXdndLeave(Display *disp, Window source, Window target)
{
	if (xdndState.xdndExchangeStarted && xdndState.amISource) {
		// Declare message struct and populate its values
		XEvent message;
		memset(&message, 0, sizeof(message));
		message.xclient.type = ClientMessage;
		message.xclient.display = disp;
		message.xclient.window = target;
		message.xclient.message_type = XdndLeave;
		message.xclient.format = 32;
		message.xclient.data.l[0] = source;
		// Rest of array members reserved so not set

		// Send it to target window
		if (XSendEvent(disp, target, False, 0, &message) == 0)
			philError("XSendEvent");
	}
}

// This is sent by the target when the exchange has completed
static void sendXdndFinished(Display *disp, Window source, Window target)
{
	if (xdndState.xdndExchangeStarted && !xdndState.amISource) {
		// Declare message struct and populate its values
		XEvent message;
		memset(&message, 0, sizeof(message));
		message.xclient.type = ClientMessage;
		message.xclient.display = disp;
		message.xclient.window = target;
		message.xclient.message_type = XdndFinished;
		message.xclient.format = 32;
		message.xclient.data.l[0] = source;
		message.xclient.data.l[1] = 1;
		message.xclient.data.l[2] = XdndActionCopy;

		// Send it to target window
		if (XSendEvent(disp, target, False, 0, &message) == 0)
			philError("XSendEvent");
	}
}

// This is sent by the target to the source to say whether or not it will accept the drop
static void sendXdndStatus(Display *disp, Window source, Window target, Atom action)
{
	if (xdndState.xdndExchangeStarted && !xdndState.amISource) {
		// Declare message struct and populate its values
		XEvent message;
		memset(&message, 0, sizeof(message));
		message.xclient.type = ClientMessage;
		message.xclient.display = disp;
		message.xclient.window = target;
		message.xclient.message_type = XdndStatus;
		message.xclient.format = 32;
		message.xclient.data.l[0] = source;
		message.xclient.data.l[1] = 1; // Sets accept and want position flags

		// Send back window rectangle coordinates and width
		message.xclient.data.l[2] = 0;
		message.xclient.data.l[3] = 0;

		// Specify action we accept
		message.xclient.data.l[4] = action;

		// Send it to target window
		if (XSendEvent(disp, target, False, 0, &message) == 0)
			philError("XSendEvent");
	}
}

// This is sent by the source to the target to say it can call XConvertSelection
static void sendXdndDrop(Display *disp, Window source, Window target)
{
	if (xdndState.xdndExchangeStarted && xdndState.amISource) {
		// Declare message struct and populate its values
		XEvent message;
		memset(&message, 0, sizeof(message));
		message.xclient.type = ClientMessage;
		message.xclient.display = disp;
		message.xclient.window = target;
		message.xclient.message_type = XdndDrop;
		message.xclient.format = 32;
		message.xclient.data.l[0] = source;
		//message.xclient.data.l[1] reserved
		message.xclient.data.l[2] = xdndState.xdndLastPositionTimestamp;

		// Send it to target window
		if (XSendEvent(disp, target, False, 0, &message) == 0)
			philError("XSendEvent");
	}
}

// This is sent by the source to the target to say the data is ready
static void sendSelectionNotify(Display *disp, XSelectionRequestEvent *selectionRequest, const char *pathStr)
{
	if (xdndState.xdndExchangeStarted && xdndState.amISource) {
		// Allocate buffer (two bytes at end for CR/NL and another for null byte)
		size_t sizeOfPropertyData = strlen("file://") + strlen(pathStr) + 3;
		char *propertyData = malloc(sizeOfPropertyData);
		if (!propertyData)
			philError("malloc");

		// Copy data to buffer
		strcpy(propertyData, "file://");
		strcat(propertyData, pathStr);
		propertyData[sizeOfPropertyData-3] = 0xD;
		propertyData[sizeOfPropertyData-2] = 0xA;
		propertyData[sizeOfPropertyData-1] = '\0';

		// Set property on target window - do not copy end null byte
		XChangeProperty(disp, selectionRequest->requestor, selectionRequest->property,
			typesWeAccept[0], 8, PropModeReplace, (unsigned char *)propertyData, sizeOfPropertyData-1);

		// Free property buffer
		free(propertyData);

		// Declare message struct and populate its values
		XEvent message;
		memset(&message, 0, sizeof(message));
		message.xselection.type = SelectionNotify;
		message.xselection.display = disp;
		message.xselection.requestor = selectionRequest->requestor;
		message.xselection.selection = selectionRequest->selection;
		message.xselection.target = selectionRequest->target;
		message.xselection.property = selectionRequest->property;
		message.xselection.time = selectionRequest->time;

		// Send it to target window
		if (XSendEvent(disp, selectionRequest->requestor, False, 0, &message) == 0)
			philError("XSendEvent");
	}
}

// Test whether the supplied atom is in our types list
static bool doWeAcceptAtom(Atom a)
{
	for (int i = 0; i < sizeof(typesWeAccept) / sizeof(Atom); ++i) {
		if (a == typesWeAccept[i]) {
			return true;
		}
	}

	return false;
}

// This gets the XdndTypeList from the source window when we need it, and then determines the
// type we will ask for
static Atom getSupportedType(Display *disp, Window source)
{
	// Try to get XdndTypeList property
	Atom retVal = None;
	Atom actualType = None;
	int actualFormat;
	unsigned long numOfItems, bytesAfterReturn;
	unsigned char *data = NULL;
	if (XGetWindowProperty(disp, source, XdndTypeList, 0, 1024, False, AnyPropertyType,
		&actualType, &actualFormat, &numOfItems, &bytesAfterReturn, &data) == Success) {
		if (actualType != None) {
			Atom *supportedAtoms = (Atom *)data;
			for (int i = 0; i < numOfItems; ++i) {
				if (doWeAcceptAtom(supportedAtoms[i])) {
					retVal = supportedAtoms[i];
					break;
				}
			}
			
			XFree(data);
		}
	}

	return retVal;
}

// Read copied path string from our window property
static char *getCopiedData(Display *disp, Window source)
{
	// Declare return value
	char *retVal = NULL;

	// Try to get PRIMARY property
	Atom actualType = None;
	int actualFormat;
	unsigned long numOfItems, bytesAfterReturn;
	unsigned char *data = NULL;
	if (XGetWindowProperty(disp, source, XDND_DATA, 0, 1024, False, AnyPropertyType,
		&actualType, &actualFormat, &numOfItems, &bytesAfterReturn, &data) == Success) {
		// Allocate temporary buffer
		char *tempBuffer = malloc(numOfItems + 1);
		if (!tempBuffer)
			philError("malloc");

		// Copy all data from X buffer then add null-byte to create proper string, then
		// dispose of X buffer
		memcpy(tempBuffer, data, numOfItems);
		tempBuffer[numOfItems] = '\0';
		XFree(data);

		// Copy from beyond 'file://' prefix if present
		char *tempPtr;
		if ((tempPtr = strstr(tempBuffer, "file://")) != NULL) {
			tempPtr = tempBuffer + 7;
		} else {
			tempPtr = tempBuffer;
		}

		// Check if cr/nl ending is present and terminate string
		// before this if so
		if (tempPtr[strlen(tempPtr)-2] == 0xD && tempPtr[strlen(tempPtr)-1] == 0xA)
			tempPtr[strlen(tempPtr)-2] = '\0';

		// Allocate return buffer
		retVal = malloc(strlen(tempPtr) + 1);
		if (!retVal)
			philError("malloc");

		// Copy data from temp buffer to it, then free temp buffer
		memcpy(retVal, tempPtr, strlen(tempPtr));
		retVal[strlen(tempPtr)] = '\0';
		free(tempBuffer);
	}

	// Return malloc allocated buffer - caller must free
	return retVal;
}

// Main logic is here
void spawnWindow(pid_t procId)
{
	// Variables
	bool continueEventLoop = true;
	bool clickedStillInWindow = false;
	Display *disp;
	const char *procStr = procId == 0 ? "Phil" : "Stuart";
	int screen, screenWidth, screenHeight, x, y;
	Window wind;
	XEvent event;
	GC gContext;
	unsigned int xdndVersion = XDND_PROTOCOL_VERSION;
	Atom *propertyList;
	int numberOfProperties;
	Square square = {
		.x = 0,
		.y = 0,
		.mouse_x = 0,
		.mouse_y = 0,
		.size = 50,
		.visible = false,
		.selected = false,
		.colour = RedSquare
	};

	// Announce function entry
	printf("%s: in spawnWindow()\n", procStr);

	// Connect to X server using DISPLAY environment variable value
	disp = XOpenDisplay(NULL);
	if (disp == NULL)
		philError("XOpenDisplay");

	// Define atoms
	XdndAware = XInternAtom(disp, "XdndAware", False);
	XA_ATOM = XInternAtom(disp, "XA_ATOM", False);
	XdndEnter = XInternAtom(disp, "XdndEnter", False);
	XdndPosition = XInternAtom(disp, "XdndPosition", False);
	XdndActionCopy = XInternAtom(disp, "XdndActionCopy", False);
	XdndLeave = XInternAtom(disp, "XdndLeave", False);
	XdndStatus = XInternAtom(disp, "XdndStatus", False);
	XdndDrop = XInternAtom(disp, "XdndDrop", False);
	XdndSelection = XInternAtom(disp, "XdndSelection", False);
	XDND_DATA = XInternAtom(disp, "XDND_DATA", False);
	XdndTypeList = XInternAtom(disp, "XdndTypeList", False);
	XdndFinished = XInternAtom(disp, "XdndFinished", False);
	WM_PROTOCOLS = XInternAtom(disp, "WM_PROTOCOLS", False);
	WM_DELETE_WINDOW = XInternAtom(disp, "WM_DELETE_WINDOW", False);

	// Define type atoms we will accept for file drop
	typesWeAccept[0] = XInternAtom(disp, "text/uri-list", False);
	typesWeAccept[1] = XInternAtom(disp, "UTF8_STRING", False);
	typesWeAccept[2] = XInternAtom(disp, "TEXT", False);
	typesWeAccept[3] = XInternAtom(disp, "STRING", False);
	typesWeAccept[4] = XInternAtom(disp, "text/plain;charset=utf-8", False);
	typesWeAccept[5] = XInternAtom(disp, "text/plain", False);

	// Get screen dimensions
	screen = DefaultScreen(disp);
	screenWidth = DisplayWidth(disp, screen);
	screenHeight = DisplayHeight(disp, screen);
	printf("%s: screen width: %d, screen height: %d\n", procStr, screenWidth, screenHeight);

	// Define colours
	unsigned long red = 0xFF << 16;
	unsigned long blue = 0xFF;
	unsigned long white = WhitePixel(disp, screen);
	unsigned long green = 0xFF << 8; // Just green component

	// Create window
	x = 0 + procId * 200;
	y = 0;
	wind = XCreateSimpleWindow(disp, RootWindow(disp, screen), x, y, 200, 200, 1,
				   red, white);
	if (wind == 0)
		philError("XCreateSimpleWindow");

	// Set window title
	if (XStoreName(disp, wind, procStr) == 0)
		philError("XStoreName");

	// Set events we are interested in
	if (XSelectInput(disp, wind, PointerMotionMask | KeyPressMask | KeyReleaseMask |
		ButtonPressMask | ButtonReleaseMask | ExposureMask | EnterWindowMask | LeaveWindowMask) == 0)
		philError("XSelectInput");

	// Add XdndAware property
	XChangeProperty(disp, wind, XdndAware,
			XA_ATOM, 32, PropModeReplace,
			(void *)&xdndVersion, 1);

	// Set WM_PROTOCOLS to add WM_DELETE_WINDOW atom so we can end app gracefully
	XSetWMProtocols(disp, wind, &WM_DELETE_WINDOW, 1);

	// Show window by mapping it
	if (XMapWindow(disp, wind) == 0)
		philError("XMapWindow");

	// Create graphics context
	gContext = XCreateGC(disp, wind, 0, NULL);
	if (gContext == 0)
		philError("XCreateGC");

	// Set foreground and background colours
	if (XSetForeground(disp, gContext, red) == 0)
		philError("XSetForeground");
	if (XSetBackground(disp, gContext, white) == 0)
		philError("XSetBackground");

	// Set square to visible if we are Phil
	if (procId == 0)
		square.visible = true;

	// Begin listening for events
	while (continueEventLoop) {
		XNextEvent(disp, &event);
		switch (event.type) {
		// We are being asked for X selection data by the target
		case SelectionRequest:
			if (xdndState.xdndExchangeStarted && xdndState.amISource) {
				// Add data to the target window
				sendSelectionNotify(disp, &event.xselectionrequest,
					saveSquareState(&square));
			}
			break;
		// We have received a selection notification
		case SelectionNotify:
			// Ignore if not XDND related
			if (event.xselection.property != XDND_DATA)
				break;

			// Temporary variables for XQueryPointer
			Window rootReturn, childReturn;
			int rootXReturn, rootYReturn, winXReturn, winYReturn;
			unsigned int maskReturn;

			// Read data out into path string
			char *pathStr = getCopiedData(disp, wind);

			// Delete property on window
			XDeleteProperty(disp, wind, XDND_DATA);

			// Set state on square
			restoreSquareState(pathStr, &square);
			square.visible = true;
			free(pathStr);
			XSetForeground(disp, gContext, square.colour == RedSquare ? red : blue);

			// Set new square coordinate origin
			XQueryPointer(disp, wind, &rootReturn, &childReturn, &rootXReturn, &rootYReturn,
				&winXReturn, &winYReturn, &maskReturn);
			square.x = winXReturn - 25;
			square.y = winYReturn - 25;
			if (square.x < 0)
				square.x = 0;
			if (square.y < 0)
				square.y = 0;
			if (square.x > 150)
				square.x = 150;
			if (square.y > 150)
				square.y = 150;

			// Send XdndFinished message
			printf("%s: sending XdndFinished\n", procStr);
			sendXdndFinished(disp, wind, xdndState.otherWindow);
			memset(&xdndState, 0, sizeof(xdndState));
			drawSquare(disp, wind, gContext, &square);
			break;
		// Motion has been detected over this window from the mouse pointer
		case MotionNotify:
			if (square.selected) {
				square.x += event.xmotion.x - square.mouse_x;
				square.y += event.xmotion.y - square.mouse_y;
				if (square.x < 0)
					square.x = 0;
				if (square.y < 0)
					square.y = 0;
				if (square.x > 150)
					square.x = 150;
				if (square.y > 150)
					square.y = 150;
				square.mouse_x = event.xmotion.x;
				square.mouse_y = event.xmotion.y;

				if (!clickedStillInWindow) {
					// Find window cursor is over
					Window targetWindow = getWindowPointerIsOver(disp, DefaultRootWindow(disp),
						event.xmotion.x_root, event.xmotion.y_root, 0, 0);
					if (targetWindow == None)
						break;

					// If cursor has moved out of previous window and cursor XDND
					// exchange is ongoing, cancel it and reset state
					if (xdndState.xdndExchangeStarted && targetWindow != xdndState.otherWindow) {
						// Send XdndLeave message
						printf("%s: sending XdndLeave message to target window 0x%x\n",
							procStr, xdndState.otherWindow);
						sendXdndLeave(disp, wind, xdndState.otherWindow);

						// Wipe state back to default
						memset(&xdndState, 0, sizeof(xdndState));
					}

					// Check state of window and engage XDND protocol exchange if needed
					if (!xdndState.xdndExchangeStarted) {
						// Check it supports XDND
						int supportsXdnd = hasCorrectXdndAwareProperty(disp, targetWindow);
						if (supportsXdnd == 0)
							break;

						// Claim ownership of Xdnd selection
						XSetSelectionOwner(disp, XdndSelection, wind, event.xmotion.time);

						// Send XdndEnter message
						printf("%s: sending XdndEnter to target window 0x%x\n",
							procStr, targetWindow);
						sendXdndEnter(disp, supportsXdnd, wind, targetWindow);
						xdndState.xdndExchangeStarted = true;
						xdndState.amISource = true;
						xdndState.otherWindow = targetWindow;
					}

					if (!xdndState.xdndStatusReceived) {
						// Send XdndPosition message
						printf("%s: sending XdndPosition to target window 0x%x\n",
							procStr, targetWindow);
						sendXdndPosition(disp, wind, targetWindow, event.xmotion.time,
							event.xmotion.x_root, event.xmotion.y_root);
					}
				}
			}
			drawSquare(disp, wind, gContext, &square);
			break;
		// Key released
		case KeyRelease:
			if (square.visible) {
				// If 'a' is pressed, alternate colour
				if (event.xkey.keycode == 38) {
					square.colour = square.colour == RedSquare ? BlueSquare : RedSquare;
					XSetForeground(disp, gContext, square.colour == RedSquare ? red : blue);
					drawSquare(disp, wind, gContext, &square);
				}
			}
			break;
		// Mouse button pressed
		case ButtonPress:
			if (isPointerInsideSquare(event.xbutton.x, event.xbutton.y, &square)) {
				// Set square properties
				square.selected = true;
				square.mouse_x = event.xbutton.x;
				square.mouse_y = event.xbutton.y;
				clickedStillInWindow = true;
				XSetForeground(disp, gContext, green);
				drawSquare(disp, wind, gContext, &square);
			}
			break;
		// Mouse button released
		case ButtonRelease:
			if (xdndState.xdndExchangeStarted && xdndState.amISource && xdndState.xdndStatusReceived) {
				// Send XdndDrop message
				printf("%s: sending XdndDrop to target window\n", procStr);
				sendXdndDrop(disp, wind, xdndState.otherWindow);
			}
			if (square.selected) {
				// Set square properties
				square.selected = false;
				XSetForeground(disp, gContext, square.colour == RedSquare ? red : blue);
				drawSquare(disp, wind, gContext, &square);
			}	
			break;
		// Redraw the window if it was covered
		case Expose:
			drawSquare(disp, wind, gContext, &square);
			break;
		// The pointer has entered our window
		case EnterNotify:
			if (square.selected) {
				clickedStillInWindow = true;
			}
			break;
		// The pointer has left our window
		case LeaveNotify:
			if (square.selected) {
				clickedStillInWindow = false;
			}
			break;
		// This is where we receive messages from the other window
		case ClientMessage:
			if (event.xclient.message_type != XdndEnter &&
				event.xclient.message_type != XdndPosition &&
				event.xclient.message_type != XdndLeave &&
				event.xclient.message_type != XdndStatus &&
				event.xclient.message_type != XdndDrop &&
				event.xclient.message_type != XdndFinished &&
				event.xclient.message_type != WM_PROTOCOLS) {
				printf("%s: received %s message\n", procStr, getEventType(&event));
				printClientMessage(disp, &event.xclient);
			}
			// Check if we are being closed
			if (event.xclient.message_type == WM_PROTOCOLS) {
				if (event.xclient.data.l[0] == WM_DELETE_WINDOW) {
					// End event loop
					continueEventLoop = false;
					break;
				}
			}

			// Check if already in XDND protocol exchange
			if (!xdndState.xdndExchangeStarted) {
				// Only handle XdndEnter messages here
				if (event.xclient.message_type == XdndEnter) {
					printf("%s: receiving XdndEnter\n", procStr);

					// Update state
					xdndState.xdndExchangeStarted = true;
					xdndState.amISource = false;
					xdndState.otherWindow = event.xclient.data.l[0];

					// Determine type to ask for
					if (event.xclient.data.l[1] & 0x1) {
						// More than three types, look in XdndTypeList
						xdndState.proposedType =
							getSupportedType(disp, xdndState.otherWindow);
					} else {
						// Only three types, check three in turn and stop when we find
						// one we support
						xdndState.proposedType = None;
						for (int i = 2; i < 5; ++i) {
							if (doWeAcceptAtom(event.xclient.data.l[i])) {
								xdndState.proposedType = event.xclient.data.l[i];
								break;
							}
						}
					}
				}
				break;
			} else {
				// Check whether we are source or target
				if (xdndState.amISource) {
					// Check for XdndStatus message
					if (event.xclient.message_type == XdndStatus) {
						xdndState.xdndStatusReceived = true;

						// Check if target will accept drop
						if ((event.xclient.data.l[1] & 0x1) != 1) {
							// Won't accept, break exchange and wipe state
							printf("%s: sending XdndLeave message to target window "
								"as it won't accept drop\n", procStr);
							sendXdndLeave(disp, wind, xdndState.otherWindow);
							memset(&xdndState, 0, sizeof(xdndState));
							break;
						}
					}
					else if (event.xclient.message_type == XdndFinished) {
						printf("%s: receiving XdndFinished message\n", procStr);
						square.visible = false;
						memset(&xdndState, 0, sizeof(xdndState));
						drawSquare(disp, wind, gContext, &square);
					}
				} else {
					// Check for XdndPosition message
					if (event.xclient.message_type == XdndPosition) {
						printf("%s: receiving XdndPosition\n", procStr);

						// Ignore if not for our window and sent erroneously
						if (xdndState.xdndPositionReceived &&
							event.xclient.data.l[0] != xdndState.otherWindow) {
							printf("%s: receiving XdndPosition from erroneous "
								"window, ignoring\n", procStr);
							break;
						}

						// Update state
						xdndState.xdndPositionReceived = true;
						xdndState.p_rootX = event.xclient.data.l[2] >> 16;
						xdndState.p_rootY = event.xclient.data.l[2] & 0xFFFF;
						xdndState.proposedAction = event.xclient.data.l[4];
						xdndState.xdndLastPositionTimestamp = event.xclient.data.l[3];

						// Now check if we should send XdndStatus message
						if (!xdndState.xdndStatusSent) {
							printf("%s: sending XdndStatus\n", procStr);
							xdndState.xdndStatusSent = true;
							sendXdndStatus(disp, wind,
								xdndState.otherWindow, xdndState.proposedAction);
						}
					}

					// Check for XdndLeave message
					if (event.xclient.message_type == XdndLeave) {
						printf("%s: receiving XdndLeave, clearing state\n", procStr);
						memset(&xdndState, 0, sizeof(xdndState));
					}

					// Check for XdndDrop message
					if (event.xclient.message_type == XdndDrop) {
						printf("%s: receiving XdndDrop, processing selection\n", procStr);

						// Ignore if not for our window and/or sent erroneously
						if (!xdndState.xdndPositionReceived ||
							event.xclient.data.l[0] != xdndState.otherWindow) {
							printf("%s: receiving XdndDrop from erroneous "
								"window, ignoring\n", procStr);
							break;
						}

						// Update state
						xdndState.xdndDropReceived = true;
						xdndState.xdndDropTimestamp = event.xclient.data.l[2];

						// Call XConvertSelection
						XConvertSelection(disp, XdndSelection, xdndState.proposedType,
							XDND_DATA, wind, xdndState.xdndDropTimestamp);
					}
				}
			}
			break;
		}
	}
	
	// Destroy window and close connection
	XFreeGC(disp, gContext);
	XDestroyWindow(disp, wind);
	XCloseDisplay(disp);

	exit(0);
}
