/*
 ---dynamic single linked list implementation---
 ---begin---
 */

struct slist {
	int len;
	struct snode *start;
};

struct snode {
	void *content;
	struct snode *next;
};

/* build an single linked list */
struct slist *slist_init();

/* add something to the end of the list */
int slist_add(struct slist *list, void *content);

/* get something from a index */
void *slist_get_at(int index, struct slist *list, int *fehler);

/* inserts an item before the index */
int slist_add_at(struct slist *list, int index, void *content);

/* prepend an item */
int slist_prepend(struct slist *list, void *content);

/* removes an item at the specified Index */
int slist_rm_at(struct slist *list, int index);

/* removes the last item */
int slist_rm_last(struct slist *list);

/* removes the node with the specified content */
int slist_rm_content(struct slist *list, void *content);

/* frees the entire datastructure including all nodes content */
int slist_free(struct slist *list);


