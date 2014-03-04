/*
 0-Software. Implements singly linked lists
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "slist.h"

#ifdef TESTX
#include <CUnit/Cunit.h>
#include <CUnit/Basic.h>
#endif

/* Initializes a structrue of type slist 
 	Return: A valid pointer to a slist structure or NULL on failure */
struct slist *slist_init(){
	struct slist *list = (struct slist *) malloc(sizeof(struct slist));
	if(list == NULL)
		return NULL;
	list->len = 0;
	list->start = NULL;
	return list;
}

/* Appends an item on the list if the list is empty, the first item will 
   be created.
 	Param: list = A vaild pointer to a slist structure
 		content = A arbitrary pointer 
 	Return: 0 on success, -1 on failure */
int slist_add(struct slist *list, void *content){
	if(list == NULL) 
		return -1;

	if((list->start) == NULL) {
		struct snode *node = (struct snode *) malloc(sizeof(struct snode));
		node->content = content;
		node->next = NULL;
		list->start = node;
	} else {
		struct snode *temp = list->start;
		while(1){
			if((temp->next) != NULL) {
				temp = temp->next;
			} else {
				struct snode *n = (struct snode *) 
										malloc(sizeof(struct snode));
				if(n == NULL)
					return -1;
				n->content = content;
				n->next = NULL;
				temp->next = n;
				break;
			}
		}
	}
	list->len++;
	return 0;
}

/* Returns the item at index-th index 
 	Param: list = A valid pointer to a slist structure
 	Return: rc=-1 and NULL on failure 
 		rc=0 and arbitrary pointer on success */
void *slist_get_at(int index, struct slist *list, int *rc){
	int i;
	if(index < 0 || list == NULL) {
		*rc = -1;
		return NULL;
	}

	if(index + 1 > list->len) {
		*rc = -1;
		return NULL;
	}

	struct snode *temp = list->start;
	for(i=0; i<index; i++) {
		temp = temp->next;
		if(temp == NULL) {
			*rc = -1;
			return NULL;
		}
	}
	
	*rc = 0;
	return temp->content;
}

/* Inserts an item at the beginning of a single linked list
	Param: list = A valid pointer to a slist structure
		content = A arbitrary pointer
	Return: 0 on success, -1 on failure */
int slist_prepend(struct slist *list, void *content){
	if(list == NULL || content == NULL)
		return -1;

	struct snode *node = (struct snode *) malloc(sizeof(struct snode));
	if(node == NULL)
		return -1;
	node->content = content;
	node->next = list->start;
	list->start = node;
	list->len++;
	return 0;
}

/* Adds an item after the index. The function appends a node only if 
	the list is empty
 	Param: list = A valid pointer to a slist structure 
 		index = The insert index
 		content = An arbitrary pointer 
	Return: 0 on success, -1 on failure */
int slist_add_at(struct slist *list, int index, void *content){
	if(list == NULL || index < 0 || content == NULL) 
		return -1;

	if((list->len) < index)
		return -1;

	if(list->len == 0 && index == 0) { 
		struct snode *node = (struct snode *) 
										malloc(sizeof(struct snode));
		if(node == NULL)
			return -1;
		node->next = NULL;
		node->content = content;
		list->start = node;
	} else {
		int i;
		struct snode *temp = list->start;
		for(i=0; i<index; i++) {
			/* catch the case that list is corrupt */
			if(temp == NULL)
				return -1;
			temp = temp->next;
		}
		struct snode *new = (struct snode *) malloc(sizeof(struct snode));
		if(new == NULL)
			return -1;

		if(temp->next == NULL)
			return -1;
		new->next = temp->next;
		new->content = content;
		temp->next = new;
	}
	list->len++;
	return 0;
}

/* Removes index-th node from the list. It does not free the space
   	occupied by the content pointer
 	Param: list = A valid pointer to a slist structure
	Return: 0 if element has been removed, -1 an error occured */
int slist_rm_at(struct slist *list, int index){
	int i; 
	struct snode *save;

	if(list == NULL || list->len < 0 || index < 0) 
		return -1;

	struct snode *temp = list->start, *prev = NULL;
	if(index == 0) {
		save = list->start;
		list->start = list->start->next;
		free(save);
		list->len--;
		return 0;
	} else {
		for(i=0; i<index; i++){
			prev = temp;
			if(temp == NULL)
				break;
			temp = temp->next;
		}
	}
	//We have to delete the last node in the list
	if(temp->next == NULL){
		prev->next = NULL;
		free(temp);
		list->len--;
		return 0;
	}

	save = temp->next;
	free(temp);
	prev->next = save;
	list->len--;
	return 0;
}

/* Removes the last item from the list
 	Param: list = A valid pointer to a slist structure 
 	Return: 0 on success, return of slist_rm_at on failure */
int slist_rm_last(struct slist *list){
	if(list == NULL) 
		return -1;
	return slist_rm_at(list, (list->len)-1);
}

/* Removes the item with content content from the list
 	Param: list = A valid pointer to a slist structure
 	Return: 0 on succes, -1 on failure */
int slist_rm_content(struct slist *list, void *content){
	if(list == NULL || content == NULL) 
		return -1;
	if(list->len == 0) {
		return -1;
	} else {
		struct snode *temp = list->start;
		struct snode *prev = NULL;
		while(temp->content != content){
			prev = temp;
			if(temp->next == NULL)
				return -1;
			temp = temp->next;
		}
		struct snode *save = temp->next;
		prev->next = save;
		free(temp);
	}
	return 0;
}

/* Frees the entire datastructure including all nodes 
   	Note, that this function does not attempt to free
	the value pointer!
 	Param: list = A valid pointer to a slist structure
 	Return: 0 on success, -1 on failure */
int slist_free(struct slist *list){
	if(list == NULL) 
		return -1;
	if(list->start == NULL) {
		free(list);
	} else {
		struct snode *temp = list->start;
		struct snode *old;
		while(1) {
			if((temp->next) == NULL) { 
				free(temp); 
				break; 
			}
			old = temp;
			temp = temp->next;
			free(old);
		}
	}
	return 0;
}

#ifdef TESTX

int init_test(void){return 0;}

void test_init(void){
	struct slist *list = slist_init();
	CU_ASSERT(list->len == 0);
	CU_ASSERT(list->start == NULL);
	CU_ASSERT(slist_free(list) == 0);
}

void test_usage(void){
	struct slist *list = slist_init();
	char **str = (char **) malloc(sizeof(char *) * 10);
	char *str3 = (char *) malloc(sizeof(char));
	char *str4 = (char *) malloc(sizeof(char));
	int i, *rc, cnt;

	/* test of functions slist_add and slist_get_at */
	for(i=0; i<10; i++){
		str[i] = (char *) malloc(sizeof(char) * 15);
		sprintf(str[i], "Ein Inhalt %i", i);
		CU_ASSERT(slist_add(list, (void *) str[i]) == 0);
	}

	for(i=0; i<10; i++){
		CU_ASSERT(!strcmp((char*) slist_get_at(i, list, rc), str[i]));
	}

	/* test of functions slist_prepend and the remove functions */
	strcpy(str3, "4711");
	CU_ASSERT(slist_prepend(list, (void *) str3) == 0);
	CU_ASSERT(!strcmp((char*) slist_get_at(0, list, rc), "4711"));

	strcpy(str4, "Einschub");
	CU_ASSERT(slist_add_at(list, 1, (void *) str4) == 0);
	CU_ASSERT(!strcmp((char *) slist_get_at(2, list,  rc), "Einschub"));
	CU_ASSERT(slist_rm_at(list, 0) == 0);
	CU_ASSERT(!strcmp((char *) slist_get_at(0, list, rc), "Ein Inhalt 0"));

	CU_ASSERT(slist_rm_at(list, 1) == 0);
	CU_ASSERT(slist_rm_last(list) == 0);

	printf("\nVollständige Liste:\n");
	for(i=0; i<list->len; i++){
		printf("Nr: %d - String: %s\n", i, slist_get_at(i, list, rc));
	}

	printf("Länge der Liste: %d\n", list->len);
	printf("Löschen aller Elemente\n");
	cnt = list->len;
	for(i=0; i<cnt; i++){
		CU_ASSERT(slist_rm_at(list,0) == 0);
	}
	printf("Länge der Liste: %d\n", list->len);
	printf("\nLeere Liste:\n");
	for(i=0; i<list->len; i++){
		printf("Nr: %d - String: %s\n", i, slist_get_at(i, list, rc));
	}
	/* further checks to come */
}

int main(int argc, char **argv){
	CU_pSuite suite = NULL;

	if(CUE_SUCCESS != CU_initialize_registry())
		return CU_get_error();

	suite = CU_add_suite("Test singly list impl", init_test, init_test);
	if(NULL == suite){
		CU_cleanup_registry();
		return CU_get_error();
	}

	if((NULL == CU_add_test(suite, "Initialisierungstest", test_init)) ||
		(NULL == CU_add_test(suite, "Hinzufügen von Elementen", test_usage))

	){
		CU_cleanup_registry();
		return CU_get_error();
	}

	/* run tests */
	CU_basic_set_mode(CU_BRM_VERBOSE);
	CU_basic_run_tests();
	CU_cleanup_registry();
	return CU_get_error();
}
#endif
