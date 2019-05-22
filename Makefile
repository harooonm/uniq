all:
	rm -f uniq
	gcc  -ffunction-sections -fdata-sections -Wl,--gc-sections -Wall\
		-Wextra -Wfatal-errors -flto -Os -s  -flto uniq.c -o uniq
