#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdint.h>

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

static int (*comp_func)(const char *s1, const char *s2, size_t n) = strncmp;

#define set_adv(x) posix_fadvise(fileno(x), 0, 0,\
	(POSIX_FADV_SEQUENTIAL|POSIX_FADV_WILLNEED))

typedef struct line{
	char *line;
	int count;
	size_t len;
}__attribute__((packed)) line_t;

static line_t *lines = NULL;
static uint64_t nr_lines = 0;

static char *trim(char *in, size_t *olen)
{
	*olen = 1;
	if (!in)
		return calloc(1, sizeof(char));

	*olen = strlen(in);
	char out[*olen + 1];
	size_t chars = 0;
	while(*in){
		if (*in != ' ')
			out[chars++] = *in;
		++in;
	}
	out[chars++] = '\0';
	char *ret = calloc(chars, sizeof(char));
	ret = strncpy(ret, out, chars);
	return ret;
}

static void find_uniq_from(FILE *f)
{
	char *line = NULL;
	size_t n = 0;
	while(getline(&line, &n, f) > 0) {
		int add = 1;
		size_t l1_len = 0;
		char *l1 = trim(line, &l1_len);

		for (uint64_t i = 0; i < nr_lines; i++) {
			char *l2 = trim(lines[i].line, &l1_len);
			l1 += skipChars;
			l2 += skipChars;
			if (!comp_func(l1, l2, compNrChars)) {
				lines[i].count++;
				add = 0;
			}
			free(l2);
		}
		free(l1);

		if (!add)
			continue;

		lines = realloc(lines, ++nr_lines * sizeof(line_t));
		lines[nr_lines - 1].line = strdup(line);
		lines[nr_lines - 1].count = 1;
		lines[nr_lines - 1].len = l1_len;
	}
	free(line);
}

int main(int argc, char **argv)
{
	char *usage = "Usage: uniq [OPTION]... [INPUT [OUTPUT]]\n\
Filter adjacent matching lines from INPUT (or standard input),\n\
writing to OUTPUT (or standard output).\n\n\
With no options, matching lines are merged to the first occurrence.\n\n\
  -c  ,    prefix lines by the count\n\
  -D M,    print all duplicate lines as groups as M method [none, pre, post]\n\
  -d  ,    only print duplicte lines, one for each group\n\
  -f N,    avoid comparing the first N fields\n\
  -i  ,    ignore case\n\
  -s N,    skip-chars\n\
  -u  ,    only print unique lines\n\
  -z  ,    line delimetere is NUL not new line\n\
  -w N,    compare no more then N characters\n\
  -h  ,    print help and exit\n";

	char terminator = '\n';
	int mask = 0;
	int optc = -1;

	while(-1 != (optc = getopt(argc, argv, "cD:df:is:uzw:h"))){
		switch(optc){
		case 'c': /*DONE*/
			mask |= COUNT_LINES;
			break;
		case 'D':
		{
			if (!strcmp("none", optarg)) {
				mask |= GRP_TYPE_NONE;
			}else if (!strcmp("pre", optarg)) {
				mask |= GRP_TYPE_PRE;
			}else if (!strcmp(optarg, "post")) {
				mask |= GRP_TYPE_POST;
			}else {
				return 0;
			}
		}
			break;
		case 'd': /*DONE*/
			mask |= DUPS_ONLY;
			break;
		case 'f':
			skipFields = atoi(optarg);
			break;
		case 'i':/*DONE*/
			mask |= IGNORE_CASE;
			break;
		case 's':/*DONE*/
			skipChars = atoi(optarg);
			break;
		case 'u':/*DONE*/
			mask |= UNIQUE_ONLY;
			break;
		case 'z':/*DONE*/
			terminator = '\0';
			break;
		case 'w': /*DONE*/
			compNrChars = atoi(optarg);
			break;
		case 'h':
			return 1;
		default:
			return 0;
		}
	}

	if (mask & IGNORE_CASE)
		comp_func = strncasecmp;


	argv += optind;
	if (!*argv) {
		if (set_adv(stdin)) {
			fprintf(stderr, "%s\n", strerror(errno));
			return 0;
		}
		find_uniq_from(stdin);
	}

	while(*argv) {
		FILE *f = fopen(*argv, "r");
		if (!f || (f && set_adv(f))) {
			fprintf(stderr, "%s %s\n", *argv, strerror(errno));
			return 0;
		}
		find_uniq_from(f);
		fclose(f);
		++argv;
	}

	for (uint64_t i = 0; i < nr_lines; i++) {
		if (((mask & DUPS_ONLY) && lines[i].count == 1)
		|| ((mask & UNIQUE_ONLY) && lines[i].count > 1))
			goto forward;

		lines[i].line[lines[i].len - 1] = terminator;
		if (mask & COUNT_LINES)
			fprintf(stdout, "    %d %s", lines[i].count, lines[i].line);
		else
			fprintf(stdout, "%s", lines[i].line);
forward:
		free(lines[i].line);
	}

	if (lines)
		free(lines);
	return 1;
}
