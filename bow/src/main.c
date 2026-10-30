#include <athena.h>
#include "bow/BowP.h"

static void sigigh_handler(int sig) {}

int main(int argc, char **argv) {
	signal(SIGINT, sigigh_handler);
	signal(SIGQUIT, sigigh_handler);
	signal(SIGHUP, sigigh_handler);
	signal(SIGTERM, sigigh_handler);
	signal(SIGPIPE, SIG_IGN);

	StartReaper();
	XInitThreads();
	ats_t ui = {0};
	ui.title = "Bow";
	ui.width = 1024;
	ui.height = 512;
	bow_t globalcres = bow_create(&ui);
	if (globalcres) {
		ui.web->priv.web = globalcres;
		bow_window(globalcres, (argc > 1 ? argv[argc - 1] : NULL), NULL);

		/*
		 * And away we go...
		 */
		ats_handler(globalcres->ats);
		ats_close(globalcres->ats);
		return 0;
	}

	return -1;
}
