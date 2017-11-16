
#include <err.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <fm/fmd_msg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>


#include <libnvpair.h>

#include "duktape.h"

/*
 * The default fatal error handler that we configure (see Makefile) in duktape.
 */
void
fatal_handler(void *udata, const char *msg)
{
	errx(1, "FATAL JAVASCRIPT ERROR: %s", msg);
}

static void
usage(void)
{
	(void) fprintf(stderr, "\nUsage: nvtool [-j] [-g field] [-i in_file]"
	    "[-e script]\n\n");
}

static int
read_nvfile(const char *fname, char **nvbuf)
{
	int fd, ret = -1;
	struct stat64 st;

	if ((fd = open(fname, O_RDONLY)) < 0 || fstat64(fd, &st) < 0) {
		(void) fprintf(stderr, "Failed to open %s for reading (%s)\n",
		    fname, strerror(errno));
		return (-1);
	}

	if (!S_ISREG(st.st_mode)) {
		(void) fprintf(stderr, "abort: %s is not a regular file\n",
		    fname);
		goto rnv_out;
	}

	if ((*nvbuf = calloc(st.st_size, 1)) == NULL) {
		(void) fprintf(stderr, "failed to allocate %u byte buffer\n",
		    (unsigned)st.st_size);
		goto rnv_out;
	}

	if (read(fd, *nvbuf, st.st_size) != st.st_size) {
		(void) fprintf(stderr, "failed to read %s in to memory\n",
		    fname);
		goto rnv_out;
	}
	ret = st.st_size;

rnv_out:
	(void) close(fd);
	return (ret);
}

/*
 * args: <nvlist pointer>, <key name>, <string value>
 */
static duk_ret_t
wrap_nvlist_add_string(duk_context *dc)
{
	nvlist_t *nvl;
	const char *key, *val;

	if ((nvl = duk_require_pointer(dc, 0)) == NULL ||
	    (key = duk_require_string(dc, 1)) == NULL ||
	    (val = duk_require_string(dc, 2)) == NULL) {
		return (DUK_RET_TYPE_ERROR);
	}

	if (nvlist_add_string(nvl, key, (char *)val) != 0) {
		err(1, "nvlist_add_string");
	}

	return (0);
}

int
main(int argc, char *argv[])
{
	int c;
	char *scripts[128];
	unsigned nscripts = 0;
	unsigned errors = 0;
	int opt_g = 0, opt_i = 0, opt_j = 0;
	char *nvfield = NULL, *in_fname = NULL;

	while ((c = getopt(argc, argv, ":e:g:i:j")) != -1) {
		switch (c) {
		case 'e':
			scripts[nscripts++] = optarg;
			break;

		case 'g':
			opt_g++;
			nvfield = optarg;
			break;

		case 'i':
			opt_i++;
			in_fname = optarg;
			break;

		case 'j':
			opt_j++;
			break;

		case ':':
			warnx("option -%c requires an operand", optopt);
			errors++;
			break;

		case '?':
			warnx("unrecognised option: -%c", optopt);
			errors++;
			break;

		default:
			errors++;
			break;
		}
	}

	if (errors > 0) {
		usage();
		exit(1);
	}

	duk_context *dc;
	if ((dc = duk_create_heap_default()) == NULL) {
		errx(1, "could not initialise Javascript interpreter");
	}

	nvlist_t *nvl;
	if (opt_i) {
		char *nvbuf;
		int nvbufsz, rv;

		/*
		 * Load nvlist from file.
		 */
		if ((nvbufsz = read_nvfile(in_fname, &nvbuf)) < 0) {
			goto out;
		}
		rv = nvlist_unpack(nvbuf, nvbufsz, &nvl, 0);
		free(nvbuf);
		if (rv != 0) {
			(void) fprintf(stderr, "failed to unpack nvlist\n");
			goto out;
		}

	} else {
		/*
		 * Start with blank nvlist.
		 */
		if (nvlist_alloc(&nvl, NV_UNIQUE_NAME, 0) != 0) {
			err(1, "nvlist_alloc");
		}
	}

	/*
	 * Install a wrapper for "nvlist_add_string" into the global
	 * context.
	 */
	duk_push_c_function(dc, wrap_nvlist_add_string, 3);
	duk_put_global_string(dc, "nvlist_add_string");

	/*
	 * Stash the nvlist in the "nvl" global.
	 */
	duk_push_pointer(dc, nvl);
	duk_put_global_string(dc, "nvl");

	/*
	 * Run each script in sequence!
	 */
	for (unsigned i = 0; i < nscripts; i++) {
		duk_eval_string_noresult(dc, scripts[i]);
	}

	duk_destroy_heap(dc);

	if (!opt_g) {
		if (opt_j) {
			(void) nvlist_print_json(stdout, nvl);
			(void) fprintf(stdout, "\n");
		} else {
			nvlist_print(stdout, nvl);
		}
	} else {
		char *val;

		if ((val = fmd_msg_decode_tokens(nvl, nvfield, "")) == NULL) {
			(void) fprintf(stderr, "failed to parse nv field "
			    "tokens\n");
			goto out;
		}
		(void) printf("%s\n", val);
		free(val);
	}

out:
	nvlist_free(nvl);

	return (0);
}
