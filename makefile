all:
	gcc -std=gnu99 -Wall -Werror -c threads.c

test:
	gcc -std=gnu99 -Wall -Werror -c threads.c
	gcc -std=gnu99 -Wall -Werror -c test.c
	gcc -std=gnu99 -o test test.o threads.o

debug:
	gcc -std=gnu99 -g -Wall -Werror -c threads.c
	gcc -std=gnu99 -g -Wall -Werror -c test.c
	gcc -std=gnu99 -g -o test test.o threads.o

clean: 
	$(RM) test test.o threads.o 