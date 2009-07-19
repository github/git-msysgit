/* common Win32 functions for MinGW and Cygwin */
#include <windows.h>
#include "../cache.h"

static inline int file_attr_to_st_mode (DWORD attr)
{
	int fMode = S_IREAD;
	if (attr & FILE_ATTRIBUTE_DIRECTORY)
		fMode |= S_IFDIR;
	else
		fMode |= S_IFREG;
	if (!(attr & FILE_ATTRIBUTE_READONLY))
		fMode |= S_IWRITE;
	return fMode;
}

static inline int get_file_attr(const char *fname, WIN32_FILE_ATTRIBUTE_DATA *fdata)
{
	if (GetFileAttributesExA(fname, GetFileExInfoStandard, fdata))
		return 0;

	switch (GetLastError()) {
	case ERROR_ACCESS_DENIED:
	case ERROR_SHARING_VIOLATION:
	case ERROR_LOCK_VIOLATION:
	case ERROR_SHARING_BUFFER_EXCEEDED:
		return EACCES;
	case ERROR_BUFFER_OVERFLOW:
		return ENAMETOOLONG;
	case ERROR_NOT_ENOUGH_MEMORY:
		return ENOMEM;
	default:
		return ENOENT;
	}
}

/*
 * make paths with special names absolute and prepend \\.\
 */
static inline const char *handle_special_name(char *buffer,
					      const char *path, int n)
{
	char cwd[PATH_MAX];
	static int initialized = 0;
	static regex_t special3, special4;
	int sep = -1, len;
	const char *psep = "\\";

	if (!enable_windows_special_names)
		return path;

	if (!initialized) {
		if (regcomp(&special3, "^(aux|con|nul|prn)(\\..*)?$",
					REG_ICASE | REG_EXTENDED) ||
		    regcomp(&special4, "^(com|lpt)[0-9](\\..*)?$",
					REG_ICASE | REG_EXTENDED))
			die ("Could not initialize special filename pattern");
		initialized = 1;
	}

	for (len = 0; path[len]; len++)
		if (is_dir_sep(path[len]))
			sep = len;

	for (len = sep + 1; path[len]; len++)
		if (path[len] == '.')
			break;

	switch (len - sep) {
		case 4:
			if (regexec(&special3, path + sep + 1, 0, NULL, 0))
				return path;
			break;
		case 5:
			if (regexec(&special4, path + sep + 1, 0, NULL, 0))
				return path;
			break;
		default:
			return path;
	}

	/* make the path absolute if not already */
	if (is_absolute_path(path) || !getcwd(cwd, PATH_MAX)) {
	    cwd[0] = '\0';
	    psep++;
	}

	/* prepend our magic spell to make windows do what we want */
	snprintf(buffer, n, "\\\\.\\%s%s%s", cwd, psep, path);

	return buffer;
}
