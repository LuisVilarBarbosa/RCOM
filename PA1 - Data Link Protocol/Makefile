CC=gcc
FLAGS=-Wall

projeto1: writenoncanonical noncanonical

writenoncanonical:
	if [ -f "writenoncanonical.c" ] ; \
	then \
		rm -f writenoncanonical ; \
		$(CC) $(FLAGS) writenoncanonical.c -o writenoncanonical ; \
	fi;

noncanonical:
	if [ -f "noncanonical.c" ] ; \
	then \
		rm -f noncanonical ; \
		$(CC) $(FLAGS) noncanonical.c -o noncanonical ; \
	fi;
