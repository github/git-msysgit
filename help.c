#include "cache.h"
#include "builtin.h"
#include "exec_cmd.h"
#include "levenshtein.h"
#include "help.h"
#include "common-cmds.h"
#include "string-list.h"
#include "column.h"
#include "version.h"

void exclude_cmds(struct string_list *cmds, struct string_list *excludes)
{
	int ci, cj, ei;
	int cmp;

	ci = cj = ei = 0;
	while (ci < cmds->nr && ei < excludes->nr) {
		cmp = strcmp(cmds->items[ci].string, excludes->items[ei].string);
		if (cmp < 0)
			cmds->items[cj++] = cmds->items[ci++];
		else if (cmp == 0)
			ci++, ei++;
		else if (cmp > 0)
			ei++;
	}

	while (ci < cmds->nr)
		cmds->items[cj++] = cmds->items[ci++];

	cmds->nr = cj;
}

static void pretty_print_string_list(struct string_list *list,
				     unsigned int colopts)
{
	struct column_options copts;

	/*
	 * always enable column display, we only consult column.*
	 * about layout strategy and stuff
	 */
	colopts = (colopts & ~COL_ENABLE_MASK) | COL_ENABLED;
	memset(&copts, 0, sizeof(copts));
	copts.indent = "  ";
	copts.padding = 2;
	print_columns(list, colopts, &copts);
	string_list_clear(list, 0);
}

static int is_executable(const char *name)
{
	struct stat st;

	if (stat(name, &st) || /* stat, not lstat */
	    !S_ISREG(st.st_mode))
		return 0;

#if defined(WIN32) || defined(__CYGWIN__)
#if defined(__CYGWIN__)
if ((st.st_mode & S_IXUSR) == 0)
#endif
{	/* cannot trust the executable bit, peek into the file instead */
	char buf[3] = { 0 };
	int n;
	int fd = open(name, O_RDONLY);
	st.st_mode &= ~S_IXUSR;
	if (fd >= 0) {
		n = read(fd, buf, 2);
		if (n == 2)
			/* DOS executables start with "MZ" */
			if (!strcmp(buf, "#!") || !strcmp(buf, "MZ"))
				st.st_mode |= S_IXUSR;
		close(fd);
	}
}
#endif
	return st.st_mode & S_IXUSR;
}

static void list_commands_in_dir(struct string_list *cmds,
					 const char *path,
					 const char *prefix)
{
	int prefix_len;
	DIR *dir = opendir(path);
	struct dirent *de;
	struct strbuf buf = STRBUF_INIT;
	int len;

	if (!dir)
		return;
	if (!prefix)
		prefix = "git-";
	prefix_len = strlen(prefix);

	strbuf_addf(&buf, "%s/", path);
	len = buf.len;

	while ((de = readdir(dir)) != NULL) {
		int entlen;

		if (prefixcmp(de->d_name, prefix))
			continue;

		strbuf_setlen(&buf, len);
		strbuf_addstr(&buf, de->d_name);
		if (!is_executable(buf.buf))
			continue;

		entlen = strlen(de->d_name) - prefix_len;
		if (has_extension(de->d_name, ".exe"))
			entlen -= 4;

		string_list_insert(cmds, de->d_name + prefix_len);
	}
	closedir(dir);
	strbuf_release(&buf);
}

void load_command_list(const char *prefix,
		struct string_list *main_cmds,
		struct string_list *other_cmds)
{
	const char *env_path = getenv("PATH");
	const char *exec_path = git_exec_path();

	if (exec_path)
		list_commands_in_dir(main_cmds, exec_path, prefix);

	if (env_path) {
		char *paths, *path, *colon;
		path = paths = xstrdup(env_path);
		while (1) {
			if ((colon = strchr(path, PATH_SEP)))
				*colon = 0;
			if (!exec_path || strcmp(path, exec_path))
				list_commands_in_dir(other_cmds, path, prefix);

			if (!colon)
				break;
			path = colon + 1;
		}
		free(paths);
	}
	exclude_cmds(other_cmds, main_cmds);
}

void list_commands(unsigned int colopts,
		   struct string_list *main_cmds, struct string_list *other_cmds)
{
	if (main_cmds->nr) {
		const char *exec_path = git_exec_path();
		printf_ln(_("available git commands in '%s'"), exec_path);
		putchar('\n');
		pretty_print_string_list(main_cmds, colopts);
		putchar('\n');
	}

	if (other_cmds->nr) {
		printf_ln(_("git commands available from elsewhere on your $PATH"));
		putchar('\n');
		pretty_print_string_list(other_cmds, colopts);
		putchar('\n');
	}
}

static int autocorrect;
static struct string_list aliases = STRING_LIST_INIT_DUP;

static int git_unknown_cmd_config(const char *var, const char *value, void *cb)
{
	if (!strcmp(var, "help.autocorrect"))
		autocorrect = git_config_int(var,value);
	/* Also use aliases for command lookup */
	if (!prefixcmp(var, "alias."))
		string_list_insert(&aliases, var + 6);

	return git_default_config(var, value, cb);
}

static int levenshtein_compare(const void *p1, const void *p2)
{
	const struct string_list_item *c1 = p1;
	const struct string_list_item *c2 = p2;
	const char *s1 = c1->string, *s2 = c2->string;
	int l1 = (int) (long int) (c1->util);
	int l2 = (int) (long int) (c2->util);

	return l1 != l2 ? l1 - l2 : strcmp(s1, s2);
}

/* An empirically derived magic number */
#define SIMILARITY_FLOOR 7
#define SIMILAR_ENOUGH(x) ((x) < SIMILARITY_FLOOR)

static const char bad_interpreter_advice[] =
	N_("'%s' appears to be a git command, but we were not\n"
	"able to execute it. Maybe git-%s is broken?");

const char *help_unknown_cmd(const char *cmd)
{
	int i, n, best_similarity = 0;
	struct string_list main_cmds = STRING_LIST_INIT_DUP,
			   other_cmds = STRING_LIST_INIT_DUP;

	git_config(git_unknown_cmd_config, NULL);

	load_command_list("git-", &main_cmds, &other_cmds);
	string_list_insert_list(&main_cmds, &aliases);
	string_list_insert_list(&main_cmds, &other_cmds);

	for (i = 0, n = 0; i < main_cmds.nr; i++) {
		int cmp = 0; /* avoid compiler stupidity */
		const char *candidate = main_cmds.items[i].string;

		/*
		 * An exact match means we have the command, but
		 * for some reason exec'ing it gave us ENOENT; probably
		 * it's a bad interpreter in the #! line.
		 */
		if (!strcmp(candidate, cmd))
			die(_(bad_interpreter_advice), cmd, cmd);

		/* Does the candidate appear in common_cmds list? */
		while (n < ARRAY_SIZE(common_cmds) &&
		       (cmp = strcmp(common_cmds[n].name, candidate)) < 0)
			n++;
		if ((n < ARRAY_SIZE(common_cmds)) && !cmp) {
			/* Yes, this is one of the common commands */
			n++; /* use the entry from common_cmds[] */
			if (!prefixcmp(candidate, cmd)) {
				/* Give prefix match a very good score */
				main_cmds.items[i].util = 0;
				continue;
			}
		}

		main_cmds.items[i].util = (void *) (long int)
			levenshtein(cmd, candidate, 0, 2, 1, 3) + 1;
	}

	qsort(main_cmds.items, main_cmds.nr,
	      sizeof(*main_cmds.items), levenshtein_compare);

	if (!main_cmds.nr)
		die(_("Uh oh. Your system reports no Git commands at all."));

	/* skip and count prefix matches */
	for (n = 0; n < main_cmds.nr && !main_cmds.items[n].util; n++)
		; /* still counting */

	if (main_cmds.nr <= n) {
		/* prefix matches with everything? that is too ambiguous */
		best_similarity = SIMILARITY_FLOOR + 1;
	} else {
		/* count all the most similar ones */
		for (best_similarity = (int) (long int) main_cmds.items[n++].util;
		     (n < main_cmds.nr &&
		      best_similarity == (int) (long int) main_cmds.items[n].util);
		     n++)
			; /* still counting */
	}
	if (autocorrect && n == 1 && SIMILAR_ENOUGH(best_similarity)) {
		/* TODO: free this string */
		const char *assumed = xstrdup(main_cmds.items[0].string);
		string_list_clear(&main_cmds, 0);
		fprintf_ln(stderr,
			   _("WARNING: You called a Git command named '%s', "
			     "which does not exist.\n"
			     "Continuing under the assumption that you meant '%s'"),
			cmd, assumed);
		if (autocorrect > 0) {
			fprintf_ln(stderr, _("in %0.1f seconds automatically..."),
				(float)autocorrect/10.0);
			poll(NULL, 0, autocorrect * 100);
		}
		return assumed;
	}

	fprintf_ln(stderr, _("git: '%s' is not a git command. See 'git --help'."), cmd);

	if (SIMILAR_ENOUGH(best_similarity)) {
		fprintf_ln(stderr,
			   Q_("\nDid you mean this?",
			      "\nDid you mean one of these?",
			   n));

		for (i = 0; i < n; i++)
			fprintf(stderr, "\t%s\n", main_cmds.items[i].string);
	}

	exit(1);
}

int cmd_version(int argc, const char **argv, const char *prefix)
{
	printf("git version %s\n", git_version_string);
	return 0;
}
