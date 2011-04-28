/*
 * Program to exercise symbolic links, which is especially interesting on Windows
 * If symbolic links are not supported in the filesystem or the user has insufficient
 * privileges, the program does a normal exit.
 */

#include "git-compat-util.h"

/* #define DEBUG 1 */
#ifdef DEBUG
#define DEBUGF(X) printf X
#else
#define DEBUGF(X) /* Nothing */
#endif

#if defined(WIN32) && defined(__MINGW32__)
#define HAVE_WIN_EXPAND_PATH 1
#endif

#define LONG_DIR_NAME "loooooooooooooooooooooooooooooooooooooooooooooooooo" \
"ooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo" \
"oooooooooooooooooooooooooooooooooooooooooooooooooooong"
#define WORKDIR "symlinks_work"


#define EQI(Got,Exp) eqi((Got),(Exp),__FILE__,__LINE__)
#define EQS(Got,Exp) eqs((Got),(Exp),__FILE__,__LINE__)
#define CHK(File,IsLink,IsDir,StatSize,LStatSize) chk((File),(IsLink),(IsDir),(StatSize),\
						      (LStatSize),__FILE__,__LINE__)
#define LST(Dir,NumDirs,NumLinks,NumFiles) lst((Dir),(NumDirs),(NumLinks),\
					       (NumFiles),__FILE__,__LINE__)

char *progname = NULL;


void rm_rf(char *name) {
	struct stat sb;
	DIR *dir;
	struct dirent *de;

	if (!strcmp(name,".") || !strcmp(name,".."))
		return;

	lstat(name,&sb);
	if (sb.st_mode & S_IFDIR) {
		chdir(name);
		if ((dir = opendir(".")) != NULL) {
			for(;;) {
				if ((de = readdir(dir)) == NULL) {
					break;
				}
				rm_rf(de->d_name);
			}
			closedir(dir);
		}
		chdir("..");

		rmdir(name);
	} else {
		unlink(name);
	}
}

void cleanup(int exitcode)
{
	chdir("..");
	rm_rf(WORKDIR);
	exit(exitcode);
}


void eqi(int got, int exp, char *file, int line)
{
	if (got != exp) {
		fprintf(stderr, "%s: expected %d, but got %d at %s:%d\n",
			progname, exp, got, file, line);
		cleanup(1);
	}
}
void eqs(char *got, char *exp, char *file, int line)
{
	if (strcmp(got,exp)) {
		fprintf(stderr,
			"%s: expected \"%s\", but got \"%s\" at %s:%d\n",
			progname, exp, got, file, line);
		cleanup(1);
	}
}

void chk(char *path, int is_link, int is_dir, int stat_size, int lstat_size, char *file, int line) {
	struct stat sb;
	memset(&sb,0,sizeof(sb));
	if(stat(path,&sb)) {
		fprintf(stderr,"%s: stat(%s) failed errno = %d at %s:%d\n",
			progname,path,errno,file,line);
		cleanup(1);
	}
	DEBUGF(("stat(%s) -> {st_mode = %x, st_ctime = %x, st_size = %d}\n",
		path,(unsigned) sb.st_mode, (unsigned) sb.st_atime,
		(int) sb.st_size));
	if (is_dir && !(sb.st_mode & S_IFDIR)) {
		fprintf(stderr,"%s: Expected %s to be a directory, but st_mode is %x at %s:%d\n",
			progname,path,sb.st_mode,file,line);
		cleanup(1);
	}
	if (stat_size >= 0 && stat_size != sb.st_size) {
		fprintf(stderr,"%s: Expected stat'd size of %s to be %d, but st_size is %d at %s:%d\n",
			progname,path,stat_size,(unsigned)sb.st_size,file,line);
		cleanup(1);
	}

	if(lstat(path,&sb)) {
		fprintf(stderr,"%s: lstat(%s) failed errno = %d at %s:%d\n",
			progname,path,errno,file,line);
		cleanup(1);
	}
	DEBUGF(("lstat(%s) -> {st_mode = %x, st_ctime = %x, st_size = %d}\n",
		path,(unsigned) sb.st_mode,
		(unsigned) sb.st_atime, (int) sb.st_size));
	if (is_link && !(sb.st_mode & S_IFLNK)) {
		fprintf(stderr,"%s: Expected %s to be a soft link, but st_mode is %x at %s:%d\n",
			progname,path,sb.st_mode,file,line);
		cleanup(1);
	}
	if (lstat_size >= 0 && lstat_size != sb.st_size) {
		fprintf(stderr,"%s: Expected stat'd size of %s to be %d, but st_size is %d at %s:%d\n",
			progname,path,lstat_size,(unsigned)sb.st_size,file,line);
		cleanup(1);
	}
}

void lst(char *path, int num_dirs, int num_links, int num_files,
	 char *file, int line)
{
	DIR *dir;
	struct dirent *de;
	int d = 0, l = 0, f = 0;
	if ((dir = opendir(path)) == NULL) {
		fprintf(stderr,
			"%s: Failed to opendir(%s), errno = %d at %s:%d\n",
			progname,path,errno,file,line);
		cleanup(1);
	} else {
		for(;;) {
			errno = 0;
			if ((de = readdir(dir)) == NULL) {
				if (errno != 0) {
					fprintf(stderr,
						"%s: Failed to readdir"
						" \"%s\", errno = %d at"
						" %s:%d\n",
						progname,path,errno,file,line);
					cleanup(1);
				}
				break;
			}
			switch ((de->d_type & (DT_LNK | DT_DIR | DT_REG))) {
			case DT_LNK:
			  ++l;
			  break;
			case DT_DIR:
			  ++d;
			  break;
			case DT_REG:
			  ++f;
			  break;
			default:
			  fprintf(stderr,
				  "%s: unexpected d_type"
				  " %d for %s at %s:%d\n",
				  progname,de->d_type,de->d_name,file,line);
			  cleanup(1);
			}
			DEBUGF(("File: %s, (%x)\n",de->d_name,
				(unsigned) de->d_type));
		}
		closedir(dir);
	}
	if (d != num_dirs) {
	  fprintf(stderr,
		  "%s: unexpected number of directories"
		  " %d (expected %d) in %s at %s:%d\n",
		  progname,d,num_dirs,path,file,line);
	  cleanup(1);
	}
	if (f != num_files) {
	  fprintf(stderr,
		  "%s: unexpected number of regular files"
		  " %d (expected %d) in %s at %s:%d\n",
		  progname,f,num_files,path,file,line);
	  cleanup(1);
	}
	if (l != num_links) {
	  fprintf(stderr,
		  "%s: unexpected number of soft links"
		  " %d (expected %d) in %s at %s:%d\n",
		  progname,l,num_links,path,file,line);
	  cleanup(1);
	}
}


int main(int argc, char **argv)
{
	char buff[PATH_MAX];
	FILE *f;
	int i;
	char s1[200],s2[200],s3[200];
	char *ptr;
	int size,size2;

	progname = argv[0];

	mkdir(WORKDIR,0777);
	chdir(WORKDIR);

	DEBUGF(("Creating directory \"a\"\n"));
	mkdir("a",0777);
	DEBUGF(("Creating directory \"a/a\"\n"));
	mkdir("a/a",0777);
	DEBUGF(("Linking b -> a\n"));
	if (symlink("a","b") != 0) {
		if (errno == ENOSYS) {
			fprintf(stderr,"%s: System does not support symbolic links, test skipped.\n",
				progname);
			cleanup(0);
		} else if (errno == EACCES) {
			fprintf(stderr,"%s: You do not have privileges to create symbolic links, "
				"test skipped.\n",
				progname);
			cleanup(0);
		} else {
			fprintf(stderr,"%s: failed to create symbolic link, unexpected errno %d.\n",
				progname,errno);
			cleanup(1);
		}
	}
	DEBUGF(("Creating text file a.txt\n"));
	f = fopen("a.txt","w");
	fprintf(f,"Random\n");
	fclose(f);
	DEBUGF(("Creating text file a/a.txt\n"));
	f = fopen("a/a.txt","w");
	fprintf(f,"RanDom\n");
	fclose(f);
	DEBUGF(("Creating text file a/a/a.txt\n"));
	f = fopen("a/a/a.txt","w");
	fprintf(f,"Random...\n");
	fclose(f);
	DEBUGF(("linking b.txt -> a.txt\n"));
	EQI(symlink("a.txt","b.txt"),0);
	DEBUGF(("linking c.txt -> b.txt\n"));
	EQI(symlink("b.txt","c.txt"),0);
	DEBUGF(("linking a/c.txt -> ../b.txt\n"));
	EQI(symlink("../b.txt","a/c.txt"),0);
	DEBUGF(("linking b/b.txt -> ../b.txt\n"));
	EQI(symlink("../b.txt","b/b.txt"),0);
	EQI(readlink("c.txt",buff,PATH_MAX),5);
	DEBUGF(("readlink(c.txt) -> %s\n",buff));
	EQS(buff,"b.txt");
	EQI(readlink("b.txt",buff,PATH_MAX),5);
	DEBUGF(("readlink(b.txt) -> %s\n",buff));
	EQS(buff,"a.txt");
	EQI(readlink("b/b.txt",buff,PATH_MAX),8);
	DEBUGF(("readlink(b/b.txt) -> %s\n",buff));
	EQS(buff,"../b.txt");
	CHK("c.txt",1,0,7,5);
	CHK("b.txt",1,0,7,5);
	CHK("b",1,1,0,1);
	CHK("a/c.txt",1,0,7,8);
	CHK("a/b.txt",1,0,7,8);
	CHK("b/b.txt",1,0,7,8);
	CHK("b/a/a.txt",0,0,10,10);

	DEBUGF(("Creating long directory name\n"));
	mkdir(LONG_DIR_NAME,0777);
	DEBUGF(("Creating long chain of links...\n"));
	for(i = 0; i < 20; ++i) {
		sprintf(s1,LONG_DIR_NAME "/a_lnk%d",i+1);
		sprintf(s2,"a_lnk%d",i+1);
		if (!i) {
			EQI(symlink("../a.txt",s1),0);
		} else {
			sprintf(s3,"../a_lnk%d",i);
			EQI(symlink(s3,s1),0);
		}
		EQI(symlink(s1,s2),0);
	}
	CHK("a_lnk20",1,0,7,strlen(LONG_DIR_NAME)+1+7);
	DEBUGF(("Creating long chain of dir links...\n"));
	for(i = 0; i < 20; ++i) {
		sprintf(s1,LONG_DIR_NAME "/a_dir%d",i+1);
		sprintf(s2,"a_dir%d",i+1);
		if (!i) {
			EQI(symlink("../a",s1),0);
		} else {
			sprintf(s3,"../a_dir%d",i);
			EQI(symlink(s3,s1),0);
		}
		EQI(symlink(s1,s2),0);
	}
	CHK("a_dir20",1,1,0,strlen(LONG_DIR_NAME)+1+7);
	CHK("a_dir20/b.txt",0,0,7,8);

	DEBUGF(("Listing dir 'a'\n"));
	LST("a",3,2,1);
	DEBUGF(("Listing dir 'a/a'\n"));
	LST("a/a",2,0,1);
	DEBUGF(("Listing dir 'b'\n"));
	LST("b",3,2,1);
	DEBUGF(("Listing dir 'b/a'\n"));
	LST("b/a",2,0,1);
	DEBUGF(("Listing directory with long link chain\n"));
	LST("a_dir20",3,2,1);
	/* Do some testing of the readlink interface and buffer size */
	ptr = NULL;
	size = readlink("b",ptr,0);
	EQI(size > 0,1);
	ptr = malloc(++size);
	EQI((size2 = readlink("b",ptr,size)) <= size,1);
	DEBUGF(("Readlink result = %d, ptr = %s\n",size2,ptr));
	CHK(ptr,0,1,0,0);
	free(ptr);
#ifdef HAVE_WIN_EXPAND_PATH
	/* Do some testing of the win_expand_path interface and buffer size */
	ptr = NULL;
	size = win_expand_path("b/a/a.txt",ptr,0);
	EQI(size > 0,1);
	ptr = malloc(++size);
	EQI((size2 = win_expand_path("b/a/a.txt",ptr,size)) <= size,1);
	DEBUGF(("Win_expand_path result = %d, ptr = %s\n",size2,ptr));
	CHK(ptr,0,0,10,10);
	free(ptr);
#endif
	DEBUGF(("Success!\n"));
	cleanup(0);
	return 0;
}
