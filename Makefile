test: keyact.c keyact.h sl_list.c slist.h
	gcc -g -o kacttest keyact.c sl_list.c -DTEST -lcunit -lpthread -lX11
