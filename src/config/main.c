#include <stdio.h>
#include <string.h>
#include "tup/config.h"

int main(int argc, char **argv)
{
	struct tup_config cfg;
	struct buf lval;
	struct buf rval;

	if(load_tup_config(&cfg) < 0) {
		fprintf(stderr, "Unable to load current configuration! Please "
			"fix .tup/config\n");
		return -1;
	}
	if(argc < 2) {
		print_tup_config(&cfg, NULL);
		return 0;
	} else if (argc != 3) {
		fprintf(stderr, "%s [param] [value]\n", argv[0]);
		fprintf(stderr, " use 'default' as value to reset param\n");
		return -1;
	}

	lval.s = argv[1];
	lval.len = strlen(argv[1]);
	rval.s = argv[2];
	rval.len = strlen(argv[2]);
	if(tup_config_set_param(&cfg, &lval, &rval) < 0)
		return -1;
	if(save_tup_config(&cfg) < 0)
		return -1;
	print_tup_config(&cfg, argv[1]);
	return 0;
}
