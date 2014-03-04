#define DELIM ", "
#define SLEEP_TIME 100


/* Library usage explained.
   ------------------------
   At first you have to initialize the Library. You can do that by
   calling the function kact_init. You get an runtime structure that is
   used by all other functions. It contains a pthread structure, a 
   mutex, and a singly linked lists for all the hotkeys.

   If you've done that, you should add some hotkeys. This can be done by 
   using the Function kact_get_hk(...) which returns an instanciated 
   keycomb structure. The member internal represents the hotkey in an
   platformindependent manner. The resulting structure pointer has to be
   passed to kact_reg_hk(...) which inserts it into the singly linked
   list. While it does this you could have startet the eventloop already
   because kact_reg_hk uses a mutex to synchronize it's access with
   the loop.

   Now it's the time you should start the event loop by calling 
   kact_start(...). It will run as long as you don't call kact_clear
   or kact_stop. While kact_stop just stops the event loop, kact_clear
   stops it too, but also removes all ressources it occupied including
   the list containing your hotkey <-> function mappings.
 */

/* depends on platform and/or api */
struct keyact {
	pthread_t *event_loop;
	pthread_mutex_t *mutex;
	Display *display;
	int cancel;
	struct slist *mapping;
};

struct hotkey {
	unsigned int keycode;
	unsigned int mod_mask;
};

struct x11_mask {
	char modstr[30];
	int mask;
};

/* platformindependent */
struct keycomb {
	int (*func)(void *mod_param);
	char *user_mod;
	int key;
	struct hotkey internal;
	void *mod_param;
};

int kact_reg_hk(struct keycomb *c, struct keyact *k);

struct keycomb *kact_get_hk(int (*func)(void *mp), const char *mod, int key, 
									void *mp);

struct keyact *kact_init();

int kact_start(struct keyact *k);

int kact_stop(struct keyact *k);

int kact_clear(struct keyact *k);

