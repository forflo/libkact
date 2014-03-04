#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <X11/Xlib.h>
#include <unistd.h>
#include "keyact.h"
#include "slist.h"

#ifdef TEST
#include <CUnit/Cunit.h>
#include <CUnit/Basic.h>
#endif 

static int transform(struct hotkey *h, struct keycomb *k);
static void search_x11(struct keyact *k, XEvent *e);

static void *event_loop(void *k);
static void loop_clean(void *k);
static void send_devent(struct keyact *k);
static int *on_error(Display *d, XErrorEvent *e);

/* Registers the hotkey c in the system k 
 	Param: c = A valid pointer to a keycomb structure. get_keycomb returns
		the necessary instance of this object.
 		k = A valid pointer to a keyact structure. An keyact structure
 		can be queried by init. 
 	Return: 0 on success and -1 on failure */
int kact_reg_hk(struct keycomb *c, struct keyact *k){
	int i;
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
	pthread_mutex_unlock(k->mutex);

	/* Register the Hotkey */
	for(i=0; i<XScreenCount(k->display); i++)
		XGrabKey(k->display, c->internal.keycode, c->internal.mod_mask,
				XRootWindow(k->display, i), 0, GrabModeAsync, GrabModeAsync);

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
struct keycomb *kact_get_hk(int (*func)(void *mp), const char *mod, int key, void *mp){
	if(func == NULL)
		return NULL;
	struct keycomb *res = (struct keycomb *) malloc(sizeof(struct keycomb));
	if(res == NULL)
		return NULL;
	res->func = func;
	res->user_mod = (char *) malloc(sizeof(char) * (strlen(mod) + 1));
	strcpy(res->user_mod, mod);	
	res->key = key;
	struct hotkey temp;
	/* Populate the hotkey structure */
	if(transform(&temp, res))
		return NULL; //TODO free!
	res->internal = temp;
	res->mod_param = mp;
	return res;
}

/* populates a given hotkey union with the x11 member content using 
 	the runtime values from k
 	Param: h = A valid (local) pointer of a hotkey union instance
 		k = A valid pointer of the current keycombination object 
 	Return: 0 on success or -1 on failure */
static int transform(struct hotkey *h, struct keycomb *k){
	if(h == NULL || k == NULL)
		return -1;
	struct x11_mask mask[] = {
		{"shift", 1<<0},
		{"lock", 1<<1},
		{"ctrl", 1<<2},
		{"mod1", 1<<3},
		{"mod2", 1<<4},
		{"mod3", 1<<5},
		{"mod4", 1<<6},
		{"mod5", 1<<7}
	};
	int i;

	Display *display = XOpenDisplay(XDisplayName(NULL));
	
	char *temp;
	char *keystr = (char *) malloc(sizeof(char));
	char *mod_copy = (char *) malloc(sizeof(char) * (strlen(k->user_mod) + 1));

	if(keystr == NULL || mod_copy == NULL)
		return -1;

	if(!display)
		return -1;

	strcpy(mod_copy, k->user_mod);
	temp = strtok(mod_copy, DELIM);
	while(1){
		if(temp == NULL)
			break;
		//iterate through mask[]. TODO remove magic number
		for(i=0; i<3; i++){
			if(!strcmp(mask[i].modstr, temp))
				h->mod_mask |= mask[i].mask;
		}
		temp = strtok(NULL, DELIM);
	}
	// no modifiers have been found
	if(h->mod_mask == 0)
		return -1;

	sprintf(keystr, "%c", (char) k->key);
	h->keycode = XKeysymToKeycode(display, XStringToKeysym(keystr));

	free(keystr);
	free(mod_copy);
	XCloseDisplay(display);
	return 0;
}

/* Returns an initialized keyact object
	Param: void
	Return: Initialized instance of struct keyact. The mutex and the singly
		linked list members are populated with valid objects. */
struct keyact *kact_init(){
	struct keyact *res = (struct keyact *) malloc(sizeof(struct keyact));
	if(res == NULL)
		return NULL;
	res->event_loop = NULL;
	res->mapping = slist_init();
	if(res->mapping == NULL)
		return NULL;

	res->display = XOpenDisplay(XDisplayName(NULL));
	if(!res->display)
		return NULL;
	res->mutex = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
	if(res->mutex == NULL)
		return NULL;
	if(pthread_mutex_init(res->mutex, NULL))
		return NULL;

	res->cancel = 0;
	return res;
}

/* Starts the main event loop. This is a convenience function, since it
 	would be very easy for the user to start the loop */
int kact_start(struct keyact *k){
	pthread_t *thread = (pthread_t *) malloc(sizeof(pthread_t));
	pthread_create(thread, NULL, event_loop, (void *) k);

	return 0;
}

/* Stops the main event loop and closes the connection to the x-server
  	 leaving everything else untouched 
 	Param: k = A Valid pointer to an keyact structure containing the 
 		display pointer 
 	Return: 0 on success -1 if an invalid value has been given as arguemnt */
int kact_stop(struct keyact *k){
	if(k == NULL)
		return -1;

	k->cancel = 1;
	send_devent(k);
	usleep(SLEEP_TIME);
	XCloseDisplay(k->display);
	
	return 0;
}

/* Sends an dummy event to a xserver. The event is of type XKeyEvent, which
	furthermore is of type KeyPress.
	Param: k = A valid pointer to an keyact structure
	Return: nothing */
static void send_devent(struct keyact *k){
	if(k == NULL)
		return;

	XEvent ev;
	XKeyEvent dummy;
	memset(&ev, 0, sizeof(XEvent));
	memset(&dummy, 0, sizeof(XKeyEvent));

	dummy.display = k->display;
	dummy.subwindow = None;
	dummy.root = None;
	dummy.time = CurrentTime;
	dummy.x = 1;
	dummy.y = 1;
	dummy.x_root = 1;
	dummy.y_root = 1;
	dummy.same_screen = True;
	dummy.window = XRootWindow(k->display, 0);
	dummy.state = None;
	dummy.keycode = None;	
	dummy.type = KeyPress;

	ev.type = KeyPress;	
	ev.xkey = dummy;

	XSendEvent(k->display, XRootWindow(k->display, 0), 0, 
			KeyPressMask, &ev);
	XFlush(k->display);
}

/* Stops the main even loop and frees all resources occupied by a 
	keyact structure including it's member mapping 
 	Param: k = A valid Pointer to a keyact structure */
int kact_clear(struct keyact *k){
	int rc = 0;
	if(k == NULL)
		return -1;

	/* We have to send an event to unblock the call to XNextEvent in
		the event_loop. TODO: propably it's better to outsource the 
		following code for better readability */
	k->cancel = 1;

	send_devent(k);
	/* wait for the thread to cancel itself */
	usleep(SLEEP_TIME);

	XCloseDisplay(k->display);

	if(k->mutex != NULL)
		rc += pthread_mutex_destroy(k->mutex);

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
	struct keycomb *temp;
	Display *display = env->display;
	XEvent event;
	char *disp_name;
	int i, *rc = (int *) malloc(sizeof(int));


	/* XAllowEvents is described in chapter 12 
		of the official Xlib documentation. 
	 	http://www.x.org/releases/X11R7.7/doc/libX11/libX11/libX11.html*/
	XAllowEvents(display, AsyncBoth, CurrentTime);

	/* Select press, release and motion events from the root windows
	 	of all screens */
	for(i=0; i<XScreenCount(display); i++)
		XSelectInput(display, XRootWindow(display, i), 
			KeyPressMask | KeyReleaseMask | ExposureMask);

	XSetErrorHandler((XErrorHandler) on_error);

	/* Traverses the slist mapping in k until it finds a fitting hotkey 
	description. 
	Param: e = A pointer to an XEvent object
		k = A pointer to an instance of struct keyact where the singly 
		list resides that has to be traversed */
	for(;;){
		XNextEvent(display, &event);
		if(env->cancel)
			break;

		/* Synchronize while operating on the list */
		pthread_mutex_lock(env->mutex);
		switch(event.type){
			case KeyPress:
				for(i=0; i<env->mapping->len; i++){
					temp = (struct keycomb *) slist_get_at(i, env->mapping, rc);
					if(*rc == -1) 
						break;

					if(temp->internal.keycode == event.xkey.keycode &&
							temp->internal.mod_mask == event.xkey.state) {
						temp->func(temp->mod_param);
						break;
					}
				}
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
		pthread_mutex_unlock(env->mutex);
	}

	pthread_exit((void *) 0);
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

#ifdef TEST

int dummy(void){
	return 0;
}

void test_init(void){
	struct keyact *env = kact_init();
	CU_ASSERT(env != NULL);
	CU_ASSERT(env->mapping != NULL);
	CU_ASSERT(env->mapping->len == 0);
	CU_ASSERT(env->event_loop == NULL);
	CU_ASSERT(env->cancel == 0);
	CU_ASSERT(env->mutex != NULL);
	CU_ASSERT(kact_clear(env) == 0);
}

int test_func(void *p){
	struct keycomb *k = (struct keycomb *) p;
	if(!strcmp(k->user_mod, "ctrl,shift")) 
		switch(k->key){
			case (int) 't':
				printf("ctrl - shift - t\n");
				break;
			case (int) 'f':
				printf("ctrl - shift - f\n");
				break;
		}
	return 0;
}

int test_func2(void *p){
	struct keycomb *k = (struct keycomb *) p;
	printf("Modifier: %s\n", k->user_mod);
	return 0;
}

void test_mod(void){
	struct keyact *env = kact_init();
	if(env == NULL)
		return;

	/* Tests for a single hotkey representation */
	struct keycomb *hk1 = kact_get_hk(test_func, "ctrl", 
							(int) 'f', (void *) 8);
	struct keycomb *hk2 = kact_get_hk(test_func, "shift", 
							(int) 'f', (void *) 8);
	struct keycomb *hk3 = kact_get_hk(test_func, "lock", 
							(int) 'f', (void *) 8);
	struct keycomb *hk4 = kact_get_hk(test_func, "mod1", 
							(int) 'f', (void *) 8);
	struct keycomb *hk5 = kact_get_hk(test_func, "mod2", 
							(int) 'f', (void *) 8);
	struct keycomb *hk6 = kact_get_hk(test_func, "mod3", 
							(int) 'f', (void *) 8);
	struct keycomb *hk7 = kact_get_hk(test_func, "mod4", 
							(int) 'f', (void *) 8);
	struct keycomb *hk8 = kact_get_hk(test_func, "mod5", 
							(int) 'f', (void *) 8);

	hk1->mod_param = (void *) hk1;
	hk2->mod_param = (void *) hk2;
	hk3->mod_param = (void *) hk3;
	hk4->mod_param = (void *) hk4;
	hk5->mod_param = (void *) hk5;
	hk6->mod_param = (void *) hk6;
	hk7->mod_param = (void *) hk7;
	hk8->mod_param = (void *) hk8;

	kact_reg_hk(hk1, env);
	kact_reg_hk(hk2, env);
	kact_reg_hk(hk3, env);
	kact_reg_hk(hk4, env);
	kact_reg_hk(hk5, env);
	kact_reg_hk(hk6, env);
	kact_reg_hk(hk7, env);
	kact_reg_hk(hk8, env);

	kact_start(env);
	sleep(1000);
	CU_ASSERT(kact_clear(env) == 0);
}

void test_usage(void){
	struct keyact *env = kact_init();
	CU_ASSERT(env != NULL);
	if(env == NULL)
		return;

	/* Tests for a single hotkey representation */
	struct keycomb *hk = kact_get_hk(test_func, "ctrl,shift", 
							(int) 't', (void *) 8);
	struct keycomb *hk2 = kact_get_hk(test_func, "ctrl,shift", 
							(int) 'f', (void *) 8);

	hk->mod_param = (void *) hk;
	hk2->mod_param = (void *) hk2;

	CU_ASSERT(hk != NULL);
	if(hk == NULL)
		return;
	CU_ASSERT(hk->func == test_func);
	CU_ASSERT(hk->user_mod != NULL);
	CU_ASSERT(hk->key == (int) 't');
	CU_ASSERT(hk->internal.keycode == 28);
	CU_ASSERT(hk->internal.mod_mask == 1<<0 | 1<<2);
	CU_ASSERT(hk->internal.mod_mask == (unsigned int) 5);
	CU_ASSERT(hk->internal.mod_mask == 5);
	CU_ASSERT(hk->mod_param == (void *) hk);
	
	CU_ASSERT(kact_reg_hk(hk, env) == 0);
	CU_ASSERT(kact_reg_hk(hk2, env) == 0);

	/* Tests for Thread starting and stopping */
	CU_ASSERT(kact_start(env) == 0);
	sleep(10);

	printf("Sende künstliches Event\n");
	/* Assemble artificial XKeyEvent */
	XEvent ev;
	XKeyEvent dummy;
	memset(&ev, 0, sizeof(XEvent));
	memset(&dummy, 0, sizeof(XKeyEvent));

	dummy.display = env->display;
	dummy.subwindow = None;
	dummy.root = None;
	dummy.time = CurrentTime;
	dummy.x = 1;
	dummy.y = 1;
	dummy.x_root = 1;
	dummy.y_root = 1;
	dummy.same_screen = True;
	dummy.window = XRootWindow(env->display, 0);
	dummy.state = hk->internal.mod_mask;
	dummy.keycode = hk->internal.keycode;	
	dummy.type = KeyPress;

	ev.type = KeyPress;	
	ev.xkey = dummy;

	XSendEvent(env->display, XRootWindow(env->display, 0), 0, 
			KeyPressMask, &ev);
	XFlush(env->display);
	/* Sleep necessary because a deadlock would occure if else */
	sleep(3);

	printf("Stoppen...\n");
	CU_ASSERT(kact_clear(env) == 0);
	//Fin	
}

/* Required calls to CUnit. Test will be registered  */
int main(int argc, char **argv){
	/* Initialize and build a Testsuite */
	CU_pSuite pSuite = NULL;

	if (CUE_SUCCESS != CU_initialize_registry())
		return CU_get_error();

	/* Creation of the main test suite */
	pSuite = CU_add_suite("Librarytest von libkeyact", dummy, dummy); 
	if (NULL == pSuite){
		CU_cleanup_registry();
		return CU_get_error();
	}

	/* Adds tests */
	if((NULL == CU_add_test(pSuite, "Initialisierungstest", test_init)) || 
		(NULL == CU_add_test(pSuite, "Usagetest2", test_mod)) || 
		(NULL == CU_add_test(pSuite, "Usagetest", test_usage))
	)

	{
		CU_cleanup_registry();
		return CU_get_error();
	}

	/* misc configuration */
	CU_basic_set_mode(CU_BRM_VERBOSE);
	CU_basic_run_tests();
	CU_cleanup_registry();
	return CU_get_error();
}

#endif
