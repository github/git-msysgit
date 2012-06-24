#ifndef HELP_H
#define HELP_H

struct string_list;

static inline void mput_char(char c, unsigned int num)
{
	while(num--)
		putchar(c);
}

extern void list_common_cmds_help(void);
extern const char *help_unknown_cmd(const char *cmd);
extern void load_command_list(const char *prefix,
			      struct string_list *main_cmds,
			      struct string_list *other_cmds);
extern void add_cmdname(struct string_list *cmds, const char *name, int len);
/* Here we require that excludes is a sorted list. */
extern void exclude_cmds(struct string_list *cmds, struct string_list *excludes);
extern void list_commands(unsigned int colopts, struct string_list *main_cmds,
			  struct string_list *other_cmds);

#endif /* HELP_H */
