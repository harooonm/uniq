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
	COUNT_LINES   = (1 << 6)
};

typedef struct line{
	char *line;
	int64_t count;
	size_t len;
}__attribute__((packed)) line_t;


static btree_t *lines = NULL;
static int skip_n_fields = 0;
static int skip_n_chars = 0;
static int cmpr_n_chars = -1;
static char terminator = '\n';
static int mask = UNIQUE_ONLY;
static FILE *file_handle = NULL;

#define in_file file_handle
#define out_file file_handle

static void print_wc(int64_t c, char *s, FILE *f)
{
	fprintf(f, "    %lu %s", c, s);
}

static void print_nc(int64_t __attribute__((unused)) c, char *s, FILE *f)
{
	fprintf(f, "%s", s);
}

static int (*str_cmp)(const char *s1, const char *s2, size_t n) = strncmp;
static void (*fprint)(int64_t c, char *s, FILE *f) = print_nc;

static char *skip_fields(char *line, int skip_n)
{
	int skipped_fields = 0;
	int field_started = 0;
	char c = '\0';
	while((c = *line)){
		switch(c){
		case ' ':
		case '\t':
			if (field_started) {
				skipped_fields += 1;
				field_started = 0;
			}
			break;
		default:
			if (!field_started)
				field_started = 1;
			if (skipped_fields >= skip_n)
				return line;
		}
		++line;
	}
	return line;
}

int cmpr_line(void *old, void *new)
{
	line_t *old_line = (line_t *)old;
	line_t *new_line = (line_t *)new;

	char *ol = old_line->line;
	char *nl = new_line->line;
	ol += skip_n_chars;
	nl += skip_n_chars;
	if (skip_n_fields){
		ol = skip_fields(ol, skip_n_fields);
		nl = skip_fields(nl, skip_n_fields);
	}
	return str_cmp(nl, ol, cmpr_n_chars);
}

static int find_uniq_from(FILE *f)
{
	/*unlikely*/
	if (posix_fadvise(fileno(f), 0, 0, POSIX_FADV_SEQUENTIAL
		| POSIX_FADV_WILLNEED)){
		fprintf(stderr, "%s\n", strerror(errno));
		return 0;
	}

	char *line = NULL;
	size_t n = 0;
	ssize_t line_len = 0;

	while((line_len = getdelim(&line, &n, terminator, f)) > 0) {
		line_t *l = calloc(1, sizeof(line_t));
		l->line = calloc(line_len + 1, sizeof(char));
		l->line = memcpy(l->line, line, line_len);
		l->line [line_len] = '\0';
		l->len = line_len;
		int added = 0;
		btree_t *old_line = add_get_tree_node(&lines, l, cmpr_line,
			&added);
		if (!added) {
			free(l->line);
			free(l);
		}
		((line_t *)(old_line->data))->count += 1;
	}
	free(line);
	return 1;
}


static void no_nl(FILE __attribute__((unused)) *f)
{
	return;
}

void (*put_before)(FILE *f) = no_nl;
void (*put_after)(FILE *f) = no_nl;

static void putnl(FILE *f)
{
	fputs("\n", f);
}

static void print_grouped(char *l, int64_t count, FILE *f)
{
	put_before(f);
	for (int64_t rc = 0; rc < count; rc++)
		fprint(count, l, f);
	put_after(f);
}

void free_line (void *data)
{
	free(((line_t *)data)->line);
	free(data);
}

static void print_lines(btree_t *t)
{
	line_t *l = (line_t *)t->data;

	l->line [l->len - 1] =  terminator;
	if ((l->count > 1 && (mask & UNIQUE_ONLY))
		|| (l->count == 1 && (mask & DUPS_ONLY)))
			return;

	if (((mask & UNIQUE_ONLY) && l->count == 1) ||
		((mask & DUPS_ONLY) && l->count > 1)){
			fprint(l->count, l->line, out_file);
			return;
	}

	print_grouped(l->line, l->count, file_handle);
}

int main(int argc, char **argv)
{
	char *usage = "Usage: uniq [OPTION]... [INPUT [OUPUT]]\n\
Filter matching lines from INPUT (or standard input),\n\
writing to OUTPUT (or standard output).\n\n\
  -c  ,    prefix lines by the count\n\
  -D N,    print all duplicate lines as groups as M method [none, pre, post]\n\
  -d  ,    only print duplicte lines, one for each group\n\
  -f N,    avoid comparing the first N fields\n\
  -i  ,    ignore case\n\
  -s N,    skip-chars\n\
  -u  ,    only print unique lines [default]\n\
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
			mask &= ~UNIQUE_ONLY;
			if (!strcmp("pre", optarg)) {
				put_before = putnl;
			} else if (!strcmp(optarg, "post")) {
				put_after = putnl;
			} else if (strcmp("none", optarg)) {
				fprintf(stderr, "%s\n", usage);
				return 0;
			}
		}
			break;
		case 'd':
			mask |= DUPS_ONLY;
			mask &= ~UNIQUE_ONLY;
			break;
		case 'f':
			skip_n_fields = atoi(optarg);
			break;
		case 'i':
			mask |= IGNORE_CASE;
			break;
		case 's':
			skip_n_chars = atoi(optarg);
			break;
		case 'z':
			terminator = '\0';
			break;
		case 'w':
			cmpr_n_chars = atoi(optarg);
			break;
		case 'h':
			fprintf(stdout, "%s\n", usage);
			return 1;
		default:
			fprintf(stderr, "%s\n", usage);
			return 0;
		}
	}

	if (mask & IGNORE_CASE)
		str_cmp = strncasecmp;

	if (mask & COUNT_LINES)
		fprint = print_wc;


	argv += optind;
	if (!*argv) {
		if (!find_uniq_from(stdin))
			return 0;
		out_file = stdout;
	} else {
		in_file = fopen(*argv, "r");
		if (!in_file) {
			fprintf(stderr, "%s %s\n", *argv, strerror(errno));
			return 1;
		}
		find_uniq_from(in_file);
		fclose(in_file);
		++argv;
		out_file = stdout;
		if (*argv)
			out_file = fopen(*argv, "w");

		if (!out_file) {
			fprintf(stderr, "%s %s\n", *argv, strerror(errno));
			return 1;
		}
	}

	itr_tree(lines, print_lines);
	free_tree(&lines, free_line);
	return 1;
}
