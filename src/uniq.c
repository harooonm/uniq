#define _DEFAULT_SOURCE

#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdint.h>
#include "libbtree.h"

enum maskBits {
	DUPS_ONLY     = (1 << 0),
	IGNORE_CASE   = (1 << 1),
	UNIQUE_ONLY   = (1 << 2),
	GRP_TYPE_NONE = (1 << 3),
	GRP_TYPE_PRE  = (1 << 4),
	GRP_TYPE_POST = (1 << 5),
	COUNT_LINES   = (1 << 6)
};

static int skipFields = 0;
static int skipChars = 0;
static int compNrChars = -1;
static char terminator = '\n';
static int mask = 0;
static int (*str_cmp)(const char *s1, const char *s2, size_t n) = strncmp;

#define set_adv(x) posix_fadvise(fileno(x), 0, 0,\
	(POSIX_FADV_SEQUENTIAL|POSIX_FADV_WILLNEED))

static btree_t *lines = NULL;

typedef struct line{
	char *line;
	uint64_t count;
	size_t len;
}__attribute__((packed)) line_t;

void free_line(void *data)
{
	free(((line_t *)data)->line);
	free(data);
}


int comp_line(void *old, void *new)
{
	line_t *old_line = (line_t *)old;
	line_t *new_line = (line_t *)new;
	return str_cmp(old_line->line, new_line->line, compNrChars);
}

static void find_uniq_from(FILE *f)
{
	if (set_adv(f)) {
		fprintf(stderr, "%s\n", strerror(errno));
		return;
	}
	char *line = NULL;
	size_t n = 0;
	ssize_t line_len = 0;
	while((line_len = getdelim(&line, &n, terminator, f)) > 0) {
		line_t *d = calloc(1, sizeof(line_t));
		d->line = strdup(line);
		d->len = line_len;
		int added = 0;
		btree_t *old_line = add_get_tree_node(&lines, d, comp_line,
			&added);

		if (!added) {
			free(d->line);
			free(d);
		}

		((line_t *)(old_line->data))->count += 1;
	}
	free(line);
}

void print_lines(btree_t *l)
{
	line_t *d = (line_t *)l->data;
	if (((mask & DUPS_ONLY) && d->count == 1)
		|| ((mask & UNIQUE_ONLY) && d->count > 1))
		return;
	d->line[d->len - 1] = terminator;
	if (mask & COUNT_LINES)
		fprintf(stdout, "    %lu %s", d->count, d->line);
	else
		fprintf(stdout, "%s", d->line);
}

int main(int argc, char **argv)
{
	char *usage = "Usage: uniq [OPTION]... [INPUT [OUTPUT]]\n\
Filter adjacent matching lines from INPUT (or standard input),\n\
writing to OUTPUT (or standard output).\n\n\
With no options, matching lines are merged to the first occurrence.\n\n\
  -c  ,    prefix lines by the count\n\
  -D N,    print all duplicate lines as groups as M method [none, pre, post]\n\
  -d  ,    only print duplicte lines, one for each group\n\
  -f N,    avoid comparing the first N fields\n\
  -i  ,    ignore case\n\
  -s N,    skip-chars\n\
  -u  ,    only print unique lines\n\
  -z  ,    line delimetere is NUL not new line\n\
  -w N,    compare no more then N characters\n\
  -h  ,    print help and exit\n";

	int optc = -1;

	while(-1 != (optc = getopt(argc, argv, "cD:df:is:uzw:h"))){
		switch(optc){
		case 'c':
			mask |= COUNT_LINES;
			break;
		case 'D':
		{
			if (!strcmp("none", optarg))
				mask |= GRP_TYPE_NONE;
			else if (!strcmp("pre", optarg))
				mask |= GRP_TYPE_PRE;
			else if (!strcmp(optarg, "post"))
				mask |= GRP_TYPE_POST;
			else
				return 0;
		}
			break;
		case 'd':
			mask |= DUPS_ONLY;
			break;
		case 'f':
			skipFields = atoi(optarg);
			break;
		case 'i':
			mask |= IGNORE_CASE;
			break;
		case 's':
			skipChars = atoi(optarg);
			break;
		case 'u':
			mask |= UNIQUE_ONLY;
			break;
		case 'z':
			terminator = '\0';
			break;
		case 'w':
			compNrChars = atoi(optarg);
			break;
		case 'h':
			fprintf(stdout, "%s\n", usage);
			return 1;
		default:
			fprintf(stdout, "%s\n", usage);
			return 0;
		}
	}

	if (mask & IGNORE_CASE)
		str_cmp = strncasecmp;


	argv += optind;
	if (!*argv)
		find_uniq_from(stdin);

	while(*argv) {
		FILE *f = fopen(*argv, "r");
		if (!f) {
			fprintf(stderr, "%s %s\n", *argv, strerror(errno));
			break;
		}
		find_uniq_from(f);
		fclose(f);
		++argv;
	}

	itr_tree(lines, print_lines);
	free_tree(&lines, free_line);
	return 1;
}