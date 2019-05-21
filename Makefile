all:
	rm -f uniq
	gcc -Os -s -flto uniq.c -o uniq
