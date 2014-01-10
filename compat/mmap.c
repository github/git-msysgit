#include "../git-compat-util.h"
#include "../cache.h"

#if defined(NO_MMAP) && NO_MMAP == OPTIONAL
#undef mmap
#undef munmap

void *git_mmap2(void *start, size_t length, int prot, int flags, int fd, off_t offset);
int git_munmap2(void *start, size_t length);

void *git_mmap(void *start, size_t length, int prot, int flags, int fd, off_t offset)
{
	return use_mmap ?
		mmap(start, length, prot, flags, fd, offset) :
		git_mmap2(start, length, prot, flags, fd, offset);
}

int git_munmap(void *start, size_t length)
{
	return use_mmap ?
		munmap(start, length) :
		git_munmap2(start, length);
}

#define git_mmap git_mmap2
#define git_munmap git_munmap2
#endif

void *git_mmap(void *start, size_t length, int prot, int flags, int fd, off_t offset)
{
	size_t n = 0;

	if (start != NULL || !(flags & MAP_PRIVATE))
		die("Invalid usage of mmap when built with NO_MMAP");

	start = xmalloc(length);
	if (start == NULL) {
		errno = ENOMEM;
		return MAP_FAILED;
	}

	while (n < length) {
		ssize_t count = pread(fd, (char *)start + n, length - n, offset + n);

		if (count == 0) {
			memset((char *)start+n, 0, length-n);
			break;
		}

		if (count < 0) {
			if (errno == EAGAIN || errno == EINTR)
				continue;
			free(start);
			errno = EACCES;
			return MAP_FAILED;
		}

		n += count;
	}

	return start;
}

int git_munmap(void *start, size_t length)
{
	free(start);
	return 0;
}
