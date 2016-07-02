/***********************************************************************
 * log.c: Handles the system calls that create files and logs them.
 ***********************************************************************
 * This file is derived from log.c in porg.
 * For more information for porg. visit http://porg.sourceforge.net
 ***********************************************************************/

#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#ifdef __APPLE__
    #define PORG_HAVE_64  0
#else
    #define PORG_HAVE_64  1
#endif

#define PORG_CHECK_INIT  do { if (!porg_tmpfile) porg_init(); } while (0)

#define PORG_BUFSIZE  4096

static int	(*libc_creat)		(const char*, mode_t);
static int	(*libc_link)		(const char*, const char*);
static int	(*libc_open)		(const char*, int, ...);
static int	(*libc_rename)		(const char*, const char*);
static int	(*libc_symlink)		(const char*, const char*);
static int	(*libc_truncate)	(const char*, off_t);
static FILE*(*libc_fopen)		(const char*, const char*);
static FILE*(*libc_freopen)		(const char*, const char*, FILE*);
#if PORG_HAVE_64
static int	(*libc_creat64)		(const char*, mode_t);
static int	(*libc_open64)		(const char*, int, ...);
static int	(*libc_truncate64)	(const char*, off64_t);
static FILE*(*libc_fopen64)		(const char*, const char*);
static FILE*(*libc_freopen64)	(const char*, const char*, FILE*);
#endif  /* PORG_HAVE_64 */

static char*	porg_tmpfile;
static int		porg_debug;

static void porg_die(const char* fmt, ...)
{
	va_list ap;
	
	fflush(stdout);
	fputs("libporg-log: ", stderr);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	putc('\n', stderr);
	
	exit(EXIT_FAILURE);
}


static void* porg_dlsym(const char* symbol)
{
	void* ret;
	char* error;

	dlerror();

	if (!(ret = dlsym(RTLD_NEXT, symbol))) {
		error = (char*)dlerror();
		porg_die("dlsym(%p, \"%s\"): %s", RTLD_NEXT, symbol,
			error ? error : "failed");
	}

	return ret;
}

		
static void porg_init()
{
	static char* dbg = NULL;

	/* handle libc */
	
	libc_creat = porg_dlsym("creat");
	libc_link = porg_dlsym("link");
	libc_open = porg_dlsym("open");
	libc_rename = porg_dlsym("rename");
	libc_symlink = porg_dlsym("symlink");
	libc_truncate = porg_dlsym("truncate");
	libc_fopen = porg_dlsym("fopen");
	libc_freopen = porg_dlsym("freopen");
#if PORG_HAVE_64
	libc_open64 = porg_dlsym("open64");
	libc_creat64 = porg_dlsym("creat64");
	libc_truncate64 = porg_dlsym("truncate64");
	libc_fopen64 = porg_dlsym("fopen64");
	libc_freopen64 = porg_dlsym("freopen64");
#endif  /* PORG_HAVE_64 */

	/* read the environment */
	
	if (!porg_tmpfile && !(porg_tmpfile = getenv("REM_LOG_TRACKING_FILE")))
		porg_die("variable %s undefined", "REM_LOG_TRACKING_FILE"); \
		
	if (!dbg && (dbg = getenv("REM_LOG_DEBUG")))
		porg_debug = !strcmp(dbg, "yes");
}


static void porg_log(const char* path, const char* dpath, const char* fmt, ...)
{
	static char abs_path[PORG_BUFSIZE];
	static char abs_path2[PORG_BUFSIZE];
	va_list a;
	int fd, len, old_errno = errno;
	
	if (!strncmp(path, "/dev/", 5) || !strncmp(path, "/proc/", 6) || !strncmp(path, "/sys/", 5))
		return;

	PORG_CHECK_INIT;

	if (porg_debug) {
		fflush(stdout);
		fprintf(stderr, "porg :: ");
		va_start(a, fmt);
		vfprintf(stderr, fmt, a);
		va_end(a);
		putc('\n', stderr);
	}
	
	/* "Absolutize" relative paths (path) */
	if (path[0] == '/') {
		strncpy(abs_path, path, PORG_BUFSIZE - 1);
		abs_path[PORG_BUFSIZE - 1] = '\0';
	}
	else if (getcwd(abs_path, PORG_BUFSIZE)) {
		strncat(abs_path, "/", PORG_BUFSIZE - strlen(abs_path) - 1);
		strncat(abs_path, path, PORG_BUFSIZE - strlen(abs_path) - 1);
	}
	else
		snprintf(abs_path, PORG_BUFSIZE, "./%s", path);
	/* "Absolutize" relative paths (dpath) */
    if(dpath) {
        if (dpath[0] == '/') {
            strncpy(abs_path2, dpath, PORG_BUFSIZE - 1);
            abs_path2[PORG_BUFSIZE - 1] = '\0';
        }
        else if (getcwd(abs_path2, PORG_BUFSIZE)) {
            strncat(abs_path2, "/", PORG_BUFSIZE - strlen(abs_path2) - 1);
            strncat(abs_path2, dpath, PORG_BUFSIZE - strlen(abs_path2) - 1);
        }
        else
            snprintf(abs_path2, PORG_BUFSIZE, "./%s", dpath);
    } else {
        abs_path2[0] = '\0';
    }

	/* strncat(abs_path, "\n", PORG_BUFSIZE - strlen(abs_path) - 1); */

    {
        static char fname[PORG_BUFSIZE];
        char *p, *q;
        unsigned int rem_log_count;
        char *rlc;
        rlc = getenv("REM_LOG_COUNT");
        rem_log_count = rlc == NULL ? 0 : atoi(rlc);
        for(p = fname, q = porg_tmpfile; *q; p++, q++) {
            if(*q == '?') {
                p += sprintf(p, "%u-", rem_log_count);
                p += sprintf(p, "%u", getpid());
                p--;
            } else {
                *p = *q;
            }
        }
        if ((fd = libc_open(fname, O_WRONLY | O_CREAT | O_APPEND, 0644)) < 0)
            porg_die("open(\"%s\"): %s", fname, strerror(errno));
    }
	
	len = strlen(abs_path);
	
    {
        static char ret[] = "\n";
        static char tab[] = "\t";
        static char line[PORG_BUFSIZE * 4];
        int len;
        len = strlen(abs_path);
        if (write(fd, abs_path, len) != len)
            porg_die("%s: write().1: %s", porg_tmpfile, strerror(errno));
        if (write(fd, tab, 1) != 1)
            porg_die("%s: write().t1: %s", porg_tmpfile, strerror(errno));
        len = strlen(abs_path2);
        if (write(fd, abs_path2, len) != len)
            porg_die("%s: write().1: %s", porg_tmpfile, strerror(errno));
        if (write(fd, tab, 1) != 1)
            porg_die("%s: write().t2: %s", porg_tmpfile, strerror(errno));
		va_start(a, fmt);
		len = vsprintf(line, fmt, a);
		va_end(a);
        if (write(fd, line, len) != len)
            porg_die("%s: write().2: %s", porg_tmpfile, strerror(errno));
        if (write(fd, ret, 1) != 1)
            porg_die("%s: write().r: %s", porg_tmpfile, strerror(errno));
    }
    /*
	if (write(fd, abs_path, len) != len)
		porg_die("%s: write(): %s", porg_tmpfile, strerror(errno));
        */
		
	if (close(fd) < 0)
		porg_die("close(%d): %s", fd, strerror(errno));
	
	errno = old_errno;
}


/************************/
/* System call handlers */
/************************/

FILE* fopen(const char* path, const char* mode)
{
	FILE* ret;
	
	PORG_CHECK_INIT;
	
	ret = libc_fopen(path, mode);
	/* if (ret && strpbrk(mode, "wa+")) */
	if (ret)
		porg_log(path, NULL, "fopen(\"%s\", \"%s\")", path, mode);
	
	return ret;
}


FILE* freopen(const char* path, const char* mode, FILE* stream)
{
	FILE* ret;
	
	PORG_CHECK_INIT;
	
	ret = libc_freopen(path, mode, stream);
	/* if (ret && strpbrk(mode, "wa+")) */
	if (ret)
		porg_log(path, NULL, "freopen(\"%s\", \"%s\")", path, mode);
	
	return ret;
}


/*
 * If newbuf isn't a directory write it to the log, otherwise log files it and
 * its subdirectories contain.
 */
static void log_rename(const char* oldpath, const char* newpath)
{
	char oldbuf[PORG_BUFSIZE], newbuf[PORG_BUFSIZE];
	struct stat st;
	DIR* dir;
	struct dirent* e;
	size_t oldlen, newlen;
	int old_errno = errno;	/* save global errno */

	/* The newpath file doesn't exist.  */
	if (-1 == lstat(newpath, &st)) 
		goto goto_end;

	else if (!S_ISDIR(st.st_mode)) {
		/* newpath is a file or a symlink.  */
		porg_log(newpath, oldpath, "rename(\"%s\", \"%s\")", oldpath, newpath);
		goto goto_end;
	}

	/* Make sure we have enough space for the following slashes.  */
	oldlen = strlen(oldpath);
	newlen = strlen(newpath);
	if (oldlen + 2 >= PORG_BUFSIZE || newlen + 2 >= PORG_BUFSIZE)
		goto goto_end;

	strcpy(oldbuf, oldpath);
	strcpy(newbuf, newpath);
	newbuf[PORG_BUFSIZE - 1] = oldbuf[PORG_BUFSIZE - 1] = '\0';

	/* We can do this in the loop below, buf it's more efficient to do
	   that once. These slashes will separate the path NEWBUF/OLDBUF
	   contains from names of its files/subdirectories.  */
	oldbuf[oldlen++] = newbuf[newlen++] = '/';
	oldbuf[oldlen] = newbuf[newlen] = '\0';

	dir = opendir(newbuf);

	while ((e = readdir(dir))) {
		if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
			continue;
		strncat(oldbuf, e->d_name, PORG_BUFSIZE - oldlen - 1);
		strncat(newbuf, e->d_name, PORG_BUFSIZE - newlen - 1);
		log_rename(oldbuf, newbuf);
		oldbuf[oldlen] = newbuf[newlen] = '\0';
	}

	closedir(dir);

goto_end:
	/* Restore global errno */
	errno = old_errno;
}


int rename(const char* oldpath, const char* newpath)
{
	int ret;
	
	PORG_CHECK_INIT;
	
	if ((ret = libc_rename(oldpath, newpath)) != -1)
		log_rename(oldpath, newpath);

	return ret;
}


int creat(const char* path, mode_t mode)
{
	int ret;
	
	PORG_CHECK_INIT;
	
	ret = libc_open(path, O_CREAT | O_WRONLY | O_TRUNC, mode);
	
	if (ret != -1)
		porg_log(path, NULL, "creat(\"%s\", 0%o)", path, (int)mode);
	
	return ret;
}


int link(const char* oldpath, const char* newpath)
{
	int ret;
	
	PORG_CHECK_INIT;
	
	if ((ret = libc_link(oldpath, newpath)) != -1)
		porg_log(newpath, NULL, "link(\"%s\", \"%s\")", oldpath, newpath);
	
	return ret;
}


int truncate(const char* path, off_t length)
{
	int ret;
	
	PORG_CHECK_INIT;
	
	if ((ret = libc_truncate(path, length)) != -1)
		porg_log(path, NULL, "truncate(\"%s\", %d)", path, (int)length);
	
	return ret;
}


int open(const char* path, int flags, ...)
{
	va_list a;
	mode_t mode;
	int accmode, ret;

    if(!porg_tmpfile && path && !strncmp(path, "/proc/", 6))
        return __open(path, flags);
    PORG_CHECK_INIT;
	
	va_start(a, flags);
	mode = va_arg(a, mode_t);
	va_end(a);
	
	if ((ret = libc_open(path, flags, mode)) != -1) {
		/*if (accmode == O_WRONLY || accmode == O_RDWR) { */
			porg_log(path, NULL, "open(\"%s\", %d)", path, flags);
        /*} */
	}

	return ret;
}


int symlink(const char* oldpath, const char* newpath)
{
	int ret;
	
	PORG_CHECK_INIT;
	
	if ((ret = libc_symlink(oldpath, newpath)) != -1)
		porg_log(newpath, oldpath, "symlink(\"%s\", \"%s\")", oldpath, newpath);
	
	return ret;
}


#if PORG_HAVE_64

int creat64(const char* path, mode_t mode)
{
	int ret;
	
	PORG_CHECK_INIT;
	
	if ((ret = libc_open64(path, O_CREAT | O_WRONLY | O_TRUNC, mode)) != -1)
		porg_log(path, NULL, "creat64(\"%s\")", path);
	
	return ret;
}


int open64(const char* path, int flags, ...)
{
	va_list a;
	mode_t mode;
	int accmode, ret;
	
    if(!porg_tmpfile && path && !strncmp(path, "/proc/", 6))
        return __open64(path, flags);
	PORG_CHECK_INIT;
	
	va_start(a, flags);
	mode = va_arg(a, mode_t);
	va_end(a);
	
	if ((ret = libc_open64(path, flags, mode)) != -1) {
		/*accmode = flags & O_ACCMODE;
		if (accmode == O_WRONLY || accmode == O_RDWR) */
        porg_log(path, NULL, "open64(\"%s\", %d)", path, flags);
	}

	return ret;
}


int truncate64(const char* path, off64_t length)
{
	int ret;
	
	PORG_CHECK_INIT;
	
	if ((ret = libc_truncate64(path, length)) != -1)
		porg_log(path, NULL, "truncate64(\"%s\", %d)", path, (int)length);
	
	return ret;
}


FILE* fopen64(const char* path, const char* mode)
{
	FILE* ret;
	
	PORG_CHECK_INIT;
	
	ret = libc_fopen64(path, mode);
	/* if (ret && strpbrk(mode, "wa+")) */
	if (ret)
		porg_log(path, NULL, "fopen64(\"%s\", \"%s\")", path, mode);
	
	return ret;
}


FILE* freopen64(const char* path, const char* mode, FILE* stream)
{
	FILE* ret;
	
	PORG_CHECK_INIT;
	
	ret = libc_freopen64(path, mode, stream);
	/* if (ret && strpbrk(mode, "wa+")) */
	if (ret)
		porg_log(path, NULL, "freopen64(\"%s\", \"%s\")", path, mode);
	
	return ret;
}

#endif  /* PORG_HAVE_64 */

