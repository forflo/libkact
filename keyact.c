#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <X11/Xlib.h>
#include "keyact.h"
#include "slist.h"

static int transform_osx(union hotkey *h, struct keycomb *k);
static int transform_win32(union hotkey *h, struct keycomb *k);
static int transform_x11(union hotkey *h, struct keycomb *k);
static void search_x11(struct keyact *k, XEvent *e);

static void *event_loop(void *k);
static int *on_error(Display *d, XErrorEvent *e);
int register_hotkey(struct keycomb *c, struct keyact *k);
struct keyact *init();
int start(struct keyact *k);

/* Registers the hotkey c in the system k 
 	Param: c = A valid pointer to a keycomb structure. get_keycomb returns
		the necessary instance of this object.
 		k = A valid pointer to a keyact structure. An keyact structure
 		can be queried by init. 
 	Return: 0 on success and -1 on failure */
int register_hotkey(struct keycomb *c, struct keyact *k){
	if(c == NULL || k == NULL)
		return -1;
	if(k->mutex == NULL)
		return -1;
	/* begin synchronisation */
	pthread_mutex_lock(k->mutex);
	if(slist_prepend(k->mapping, (void*) c)){
		pthread_mutex_unlock(k->mutex);
		return -1;
	}
	/* proper unlocking */
	pthread_mutex_unlock(k->mutex);
	return 0;
}

/* Returns an object of type struct keycomb. This object is later
 	used in the main event loop to identify an key combination.
 	Because the representation of key combination differs on the
	single systems, the layer keycomb has to be used. Keycomb acts
	as a platformindependent representation of hotkeys associated
	with functions. The union member internal holds three different
	structures. 
 	Param: func = A valid Functionpointer which should be associated
 		with the given hotkey
 		mod = A comma or space separated list of modifier substrings.
 			e.g. "strg,alt,shift". Valid substrings are the following
 				- "strg"
 				- "alt"
 				- "shift"
 		key = A character 
 		mp = A arbitrary pointer. This pointer will be passed to the 
		function func
	Return: A new object of type struct keycomb or NULL if an error
		occured */
struct keycomb *get_keycomb(int (*func)(void *mp), char *mod, int key, void *mp){
	if(func == NULL)
		return NULL;
	struct keycomb *res = (struct keycomb *) malloc(sizeof(struct keycomb));
	if(res == NULL)
		return NULL;
	res->func = func;
	res->user_mod = mod;
	res->key = key;
	union hotkey temp;
	/* Populate the hotkey union */
	switch(SYS){
		case x11:
			if(transform_x11(&temp, res))
				return NULL; //TODO free!
		case osx:
			if(transform_win32(&temp, res))
				return NULL;
		case win:
			if(transform_osx(&temp, res))
				return NULL;
	}
	res->internal = temp;
	res->mod_param = mp;
	return res;
}

/* populates a given hotkey union with the x11 member content using 
 	the runtime values from k
 	Param: h = A valid (local) pointer of a hotkey union instance
 		k = A valid pointer of the current keycombination object 
 	Return: 0 on success or -1 on failure */
static int transform_x11(union hotkey *h, struct keycomb *k){
	if(h == NULL || k == NULL)
		return -1;
	struct x11_mask mask[] = {
		{"shift", 1<<0},
		{"lock", 1<<1},
		{"ctrl", 1<<2}
	};
	struct keycomb_x11 x11;
	int i;

	Display *display = XOpenDisplay(XDisplayName(NULL));
	
	char *temp;
	char *keystr = (char *) malloc(sizeof(char));
	if(keystr == NULL)
		return -1;

	if(!display)
		return -1;

	temp = strtok(k->user_mod, DELIM);
	while(1){
		if(temp == NULL)
			break;
		//iterate through mask[]. TODO remove magic number
		for(i=0; i<3; i++){
			if(!strcmp(mask[i].modstr, temp))
				x11.mod_mask |= mask[i].mask;
		}
		temp = strtok(k->user_mod, DELIM);
	}
	// no modifiers have been found
	if(x11.mod_mask == 0)
		return -1;

	sprintf(keystr, "%c", (char) k->key);
	x11.keycode = XKeysymToKeycode(display, XStringToKeysym(keystr));
	h->x11 = x11;

	free(keystr);
	return 0;
}

static int transform_osx(union hotkey *h, struct keycomb *k){
	//not yet implemented
}

static int transform_win32(union hotkey *h, struct keycomb *k){
	//not yet implemented
}

/* Returns an initialized keyact object
	Param: void
	Return: Initialized instance of struct keyact. The mutex and the singly
		linked list members are populated with valid objects. */
struct keyact *init(){
	struct keyact *res = (struct keyact *) malloc(sizeof(struct keyact));
	if(res == NULL)
		return NULL;
	res->event_loop = NULL;
	res->mapping = slist_init();
	if(res->mapping == NULL)
		return NULL;

	if(pthread_mutex_init(res->mutex, NULL))
		return NULL;
}

/* Starts the main event loop. This is a convenience function, since it
 	would be very easy for the user to start the loop */
int start(struct keyact *k){
	pthread_t *thread;
	pthread_create(thread, NULL, event_loop, (void *) k);

	return 0;
}

/* Stops the main event loop */
int stop(struct keyact *k){
	return pthread_cancel(*k->event_loop);
}

/* Stops the main even loop and frees all resources occupied by a 
	keyact structure including it's member mapping */
int clear(struct keyact *k){
	int rc;
	rc += pthread_mutex_destroy(k->mutex);
	rc += pthread_cancel(*k->event_loop);
	rc += slist_free(k->mapping);
	free(k);
	return rc;
}

/* The main event loop which queries the environment for keystrokes and
 	hotkeys. If a hotkey occures, the corresponding function will be 
 	called.
 	Param: k = A valid pointer to a keyact structure.
 	Return: (void *) -1 on failure or (void*) 0 on success */
static void *event_loop(void *k){
	struct keyact *env = (struct keyact *) k;
	Display *display;
	XEvent event;
	char *disp_name;
	int i;

	/* Open Display and configure events and input */		
	disp_name = XDisplayName(NULL);
	display = XOpenDisplay(disp_name);
	if(!display)
		pthread_exit((void *)-1);

	/* XAllowEvents is described in chapter 12 
		of the official Xlib documentation. 
	 	http://www.x.org/releases/X11R7.7/doc/libX11/libX11/libX11.html*/
	XAllowEvents(display, AsyncBoth, CurrentTime);

	/* Select press, release and motion events from the root windows
	 	of all screens */
	for(i=0; i<ScreenCount(display); i++)
		XSelectInput(display, RootWindow(display, i), 
				KeyPressMask | KeyReleaseMask | PointerMotionMask);

	XSetErrorHandler((XErrorHandler) on_error);

	/* Main event loop */
	while(1){
		XNextEvent(display, &event);
		
		switch(event.type){
			case KeyPress:
				search_x11(env, &event);
				break;
			case KeyRelease:

				break;
			case ButtonPress:
				//currently not implemented
				break;
			case ButtonRelease:
				//same as above
				break;
			default:
				break;
		}
	}
}

/* Traverses the slist mapping in k until it finds a fitting hotkey 
 	description. 
 	Param: e = A pointer to an XEvent object
 		k = A pointer to an instance of struct keyact where the singly 
 		list resides that has to be traversed */
static void search_x11(struct keyact *k, XEvent *e){
	int i, *rc;
	struct keycomb *temp;
	for(i=0; i<k->mapping->len; i++){
		temp = (struct keycomb *) slist_get_at(i, k->mapping, rc);
		if(temp->internal.x11.keycode == e->xkey.keycode &&
			temp->internal.x11.mod_mask == e->xkey.state) {
			/* run the function only once */
			temp->func(temp->mod_param);
			break;
		}
	}
}

/* Little Errorhandler for the X11-System. It currently does nothing */
static int *on_error(Display *d, XErrorEvent *e){
	static int already = 0;
	if(already != 0)
		return NULL;
	already = 1;

	//TODO: Error handling...
	return NULL;
}

