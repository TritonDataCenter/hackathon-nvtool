
#include <err.h>
#include <unistd.h>

#include <libnvpair.h>

#include "duktape.h"

void
fatal_handler(void *udata, const char *msg)
{
	errx(1, "FATAL JAVASCRIPT ERROR: %s", msg);
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

	while ((c = getopt(argc, argv, ":e:")) != -1) {
		switch (c) {
		case 'e':
			scripts[nscripts++] = optarg;
			break;

		case ':':
			warnx("option -%c requires an operand", optopt);
			errors++;
			break;

		case '?':
			warnx("unrecognised option: -%c", optopt);
			errors++;
			break;
		}
	}

	if (errors > 0) {
		errx(1, "usage: %s -e SCRIPT [-e SCRIPT ...]", argv[0]);
	}

	duk_context *dc;
	if ((dc = duk_create_heap_default()) == NULL) {
		errx(1, "could not initialise Javascript interpreter");
	}

	nvlist_t *nvl;
	if (nvlist_alloc(&nvl, NV_UNIQUE_NAME, 0) != 0) {
		err(1, "nvlist_alloc");
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

	nvlist_print(stdout, nvl);

	return (0);
}
