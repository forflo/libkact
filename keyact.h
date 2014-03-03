#define DELIM ", "

#define X11
#ifdef X11
#define SYS 0
#endif

#ifdef OSX
#define SYS 1
#endif

#ifdef WIN
#define SYS 2
#endif

enum sys {x11 = 0, osx, win};

struct keyact {
	pthread *event_loop;
	pthread_mutex_t *mutex;
	struct slist *mapping;
};

struct keycomb_x11 {
	unsigned int keycode;
	unsigned int mod_mask;
};

/* osx and win32 not yet implemented */
struct keycomb_osx {
	unsigned int keycode;
};

struct keycomb_win32 {
	unsigned int keycode;
};

union hotkey {
	struct keycomb_x11 x11;
	struct keycomb_osx osx;
	struct keycomb_win32 win32;
};

struct keycomb {
	int (*func)(void *mod_param);
	char *user_mod;
	int key;
	union hotkey internal;
	void *mod_param;
};

int register_hotkey(struct keycomb *c, struct keyact *k);

struct keycomb *get_keycomb(int (*func)(void *mp), char *mod, int key, void *mp);

struct keyact *init();

int start(struct keyact *k);

int stop(struct keyact *k);

struct x11_mask {
	char modstr[30];
	int mask;
};
