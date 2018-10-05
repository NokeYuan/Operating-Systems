all : dolist dosync handoff

dolist : list_driver.c list.c list.h
	gcc -Wall -g -o dolist list_driver.c list.c -lpthread
	
dosync : list_driver.c list_sync.c list.h
	gcc -Wall -g -o dosync list_driver.c list_sync.c -lpthread
	
handoff : list_driver.c list_handoff.c list.h
	gcc -Wall -g -o handoff list_driver.c list_handoff.c -lpthread

clean:
	rm dolist dosync handoff
