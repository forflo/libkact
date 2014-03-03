/*
 0-Software. Implements dynamically growing arrays with arbitrary content
 */

#include "parser.h"
#include <string.h>
#include <stdlib.h>


#ifdef TEST
#include <CUnit/Basic.h>
#include <CUnit/Cunit.h>

#define MAX 10
#endif

/* Initializes a dyn_arr structure 
 	Param: void
 	Return: a pointer to a valid dyn_arr structure or NULL on 
 		failure */
struct dyn_arr *arr_init(){
	struct dyn_arr *arr = (struct dyn_arr *) malloc(sizeof(struct dyn_arr));
	if(arr == NULL)
		return NULL;
	arr->num = 0;
	arr->arr = NULL;
	return arr;
}

/* Adds a value structure to an specified identifier table 
 	Param: table = Valid pointer to an id table
 		val = Pointer to a value structure
 		id = Pointer to the string which is the id
 	Return: 0 on success or -1 failure */
int arr_add_value(struct dyn_arr *arr, void *val){
	if(arr == NULL || val == NULL)
		return -1;

	arr->arr = (void **) realloc(arr->arr, sizeof(void *) * 
													((arr->num) + 1));
	if(arr->arr == NULL)
		return -1;

	arr->arr[arr->num++] = val;
	return 0;
}

/* Just for user convenience. Getting a value from a dyn_arr strucutre
 	is as easy as accessing it's arr field with an array selector like
 	Param: arr = Pointer to a valid dyn_arr structure
 		index = Index of the requested object 
 	Return: void* = the requested object */
//Untested...
void *arr_get_value(struct dyn_arr *arr, int index){
	if(arr == NULL || index < 0)
		return NULL;
	if(index > arr->num)
		return NULL;

	return arr->arr[index];
}

/* Completely frees a dynamic array including all of the pointers stored
 	in it.
 	Param: arr = Pointer to a valid dyn_arr structure
 	Return: -1 on failure, 0 on success. */
int arr_free(struct dyn_arr *arr){
	if(arr==NULL)
		return -1;
	int i;
	for(i=0; i<arr->num; i++)
		free(arr->arr[i]);

	free(arr->arr);
	free(arr);

	return 0;
}

#ifdef TEST

int init_test(void){return 0;}

void test_init(void){
	struct dyn_arr *test1 = arr_init();
	CU_ASSERT(test1->num == 0);
	CU_ASSERT(test1->arr == NULL);
	CU_ASSERT(arr_free(test1)==0);
}

void test_usage(void){
	struct dyn_arr *test = arr_init();
	char *temp;
	int i;

	for(i=0; i<MAX; i++){
		temp = (char *) malloc(sizeof(char));
		sprintf(temp, "%d", i);
		CU_ASSERT(arr_add_value(test, (void *) temp) == 0);
	}

	for(i=0; i<MAX; i++){
		temp = (char *) malloc(sizeof(char));
		sprintf(temp, "%d", i);
		CU_ASSERT(!strcmp(test->arr[i], temp));
	}
	
	CU_ASSERT(arr_free(test)==0);
}

int main(int argc, char **argv){
	CU_pSuite suite = NULL;

	if(CUE_SUCCESS != CU_initialize_registry())
		return CU_get_error();

	suite = CU_add_suite("Test dynamic array", init_test, init_test);
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
