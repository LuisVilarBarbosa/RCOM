CC=gcc
FLAGS=-Wall

projeto1: writenoncanonical noncanonical

writenoncanonical:
	if [ -f "writenoncanonical.c" ] ; \
	then \
		$(CC) $(FLAGS) writenoncanonical.c -o writenoncanonical ; \
	fi;

noncanonical:
	if [ -f "noncanonical.c" ] ; \
	then \
		$(CC) $(FLAGS) noncanonical.c -o noncanonical ; \
	fi;
