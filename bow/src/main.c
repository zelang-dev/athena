/*
 * main.c
 *
 * Copyright (C) 1993-1997, John Kilburg <john@cs.unlv.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <athena.h>
#include "bow/BowP.h"
#include "bow_archer.xpm"

static void sigigh_handler(int sig) {}

int main(int argc, char **argv) {
	char base_url[255];

	signal(SIGINT, sigigh_handler);
	signal(SIGQUIT, sigigh_handler);
	signal(SIGHUP, sigigh_handler);
	signal(SIGTERM, sigigh_handler);
	signal(SIGPIPE, SIG_IGN);

	StartReaper();
	XInitThreads();
	ats_t ui = {0};
	ui.use_icon = bow_archer;
	ui.width = 1024;
	ui.height = 512;
	bow_t globalcres = bow_create(&ui, NULL);
	if (globalcres) {
		/*
		* Default base URL; this allows filenames to be used on the
		* command line
		*/
		strcpy(base_url, "file:");
		getcwd(base_url + 5, sizeof(base_url) - 5);
		strcat(base_url, "/");

		if (argc > 1)
			HeadCreate(globalcres, RequestCreate(globalcres, argv[argc - 1], base_url), NULL);
		else
			HeadCreate(globalcres, NULL, NULL);

		/*
		 * And away we go...
		 */
		ats_handler(globalcres->ats);
		ats_close(globalcres->ats);
		return 0;
	}

	return -1;
}
