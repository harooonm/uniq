#define _DEFAULT_SOURCE

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
	COUNT_LINES   = (1 << 6)
};

static int skip_n_fields = 0;
static int skip_n_chars = 0;
static int cmpr_n_chars = -1;
static char terminator = '\n';
static int mask = UNIQUE_ONLY;

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

typedef struct line{
	char *line;
	int64_t count;
	size_t len;
}__attribute__((packed)) line_t;


static line_t *lines = NULL;
static int64_t nr_lines = 0;

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

int cmpr_line(char *old, char *new)
{
	old += skip_n_chars;
	new += skip_n_chars;

	if (skip_n_fields) {
		old = skip_fields(old, skip_n_fields);
		new = skip_fields(new, skip_n_fields);
	}

	return str_cmp(old, new, cmpr_n_chars);
}

static void find_uniq_from(FILE *f)
{
	if (posix_fadvise(fileno(f), 0, 0, POSIX_FADV_SEQUENTIAL
		| POSIX_FADV_WILLNEED)){
		fprintf(stderr, "%s\n", strerror(errno));
		return;
	}
	char *line = NULL;
	size_t n = 0;
	ssize_t line_len = 0;

	while((line_len = getdelim(&line, &n, terminator, f)) > 0) {

		int64_t tail = nr_lines - 1;
		int64_t head = 0;
		int64_t found = -1;

		while(1 && nr_lines) {

			if (head > nr_lines || head > tail)
				break;

			if (tail  < 0 || tail < head)
				break;

			if (cmpr_line(lines[head].line, line) == 0) {
				found = head;
				break;
			}

			if (head != tail && cmpr_line(lines[tail].line, line) == 0) {
				found = tail;
				break;
			}
			--tail;
			++head;
		}

		if (found != -1) {
			lines[found].count += 1;
			continue;
		}

		lines = realloc(lines, ++nr_lines * sizeof(line_t));
		lines[nr_lines - 1].line = calloc(line_len + 1, sizeof(char));
		lines[nr_lines - 1].line = memcpy(lines[nr_lines - 1].line, line, line_len);
		lines[nr_lines - 1].line[line_len] = '\0';
		lines[nr_lines - 1].len  = line_len;
		lines [nr_lines - 1].count += 1;
	}
	free(line);
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

static void print_grouped(line_t l, FILE *f)
{
	put_before(f);
	for (int64_t rc = 0; rc < l.count; rc++)
		fprint(l.count, l.line, f);
	put_after(f);
}

static void print_lines(FILE *f)
{
	for (int64_t i = 0; i < nr_lines; i++) {
		line_t l = lines[i];
		if ((l.count > 1 && (mask & UNIQUE_ONLY))
		|| (l.count == 1 && (mask & DUPS_ONLY)))
			continue;

		if (((mask & UNIQUE_ONLY) && l.count == 1) ||
			((mask & DUPS_ONLY) && l.count > 1)) {
			fprint(l.count, l.line, f);
			continue;
		}

		print_grouped(l, f);
	}
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
		find_uniq_from(stdin);
		print_lines(stdout);
	} else {
		FILE *f = fopen(*argv, "r");
		if (!f) {
			fprintf(stderr, "%s %s\n", *argv, strerror(errno));
			return 1;
		}
		find_uniq_from(f);
		fclose(f);
		f = stdout;
		++argv;
		if (*argv)
			f = fopen(*argv, "w");
		print_lines(f);
		fflush(f);
		fclose(f);
	}
	return 1;
}
