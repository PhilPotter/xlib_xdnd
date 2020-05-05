/* Copyright Phillip Potter, 2020 - MIT License */
#include <X11/Xlib.h>

/* This turns events into strings */
const char *getEventType(XEvent *event)
{
	const char *retVal;
	switch (event->type) {
	case 2:
		retVal = "KeyPress";
		break;
	case 3:
		retVal = "KeyRelease";
		break;
	case 4:
		retVal = "ButtonPress";
		break;
	case 5:
		retVal = "ButtonRelease";
		break;
	case 6:
		retVal = "MotionNotify";
		break;
	case 7:
		retVal = "EnterNotify";
		break;
	case 8:
		retVal = "LeaveNotify";
		break;
	case 9:
		retVal = "FocusIn";
		break;
	case 10:
		retVal = "FocusOut";
		break;
	case 11:
		retVal = "KeymapNotify";
		break;
	case 12:
		retVal = "Expose";
		break;
	case 13:
		retVal = "GraphicsExpose";
		break;
	case 14:
		retVal = "NoExpose";
		break;
	case 15:
		retVal = "VisibilityNotify";
		break;
	case 16:
		retVal = "CreateNotify";
		break;
	case 17:
		retVal = "DestroyNotify";
		break;
	case 18:
		retVal = "UnmapNotify";
		break;
	case 19:
		retVal = "MapNotify";
		break;
	case 20:
		retVal = "MapRequest";
		break;
	case 21:
		retVal = "ReparentNotify";
		break;
	case 22:
		retVal = "ConfigureNotify";
		break;
	case 23:
		retVal = "ConfigureRequest";
		break;
	case 24:
		retVal = "GravityNotify";
		break;
	case 25:
		retVal = "ResizeRequest";
		break;
	case 26:
		retVal = "CirculateNotify";
		break;
	case 27:
		retVal = "CirculateRequest";
		break;
	case 28:
		retVal = "PropertyNotify";
		break;
	case 29:
		retVal = "SelectionClear";
		break;
	case 30:
		retVal = "SelectionRequest";
		break;
	case 31:
		retVal = "SelectionNotify";
		break;
	case 32:
		retVal = "ColormapNotify";
		break;
	case 33:
		retVal = "ClientMessage";
		break;
	case 34:
		retVal = "MappingNotify";
		break;
	case 35:
		retVal = "GenericEvent";
		break;
	default:
		retVal = "Unknown";
		break;
	}

	return retVal;
}
