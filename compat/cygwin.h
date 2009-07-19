#include <sys/types.h>
#include <sys/stat.h>

typedef int (*stat_fn_t)(const char*, struct stat*);
extern stat_fn_t cygwin_stat_fn;
extern stat_fn_t cygwin_lstat_fn;

#define stat(path, buf) (*cygwin_stat_fn)(path, buf)
#define lstat(path, buf) (*cygwin_lstat_fn)(path, buf)

/*
 * replacements of existing functions
 */

int cygwin_unlink(const char *pathname);
#define unlink cygwin_unlink

int cygwin_open (const char *filename, int oflags, ...);
#define open cygwin_open

int cygwin_utime(const char *file_name, const struct utimbuf *times);
#define utime cygwin_utime

int cygwin_rename(const char*, const char*);
#define rename cygwin_rename

int cygwin_rmdir(const char *path);
#define rmdir cygwin_rmdir

int cygwin_link(const char *oldpath, const char *newpath);
#define link cygwin_link

int cygwin_mkdir(const char *path, int mode);
#define mkdir cygwin_mkdir
