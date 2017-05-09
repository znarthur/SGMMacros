#ifndef lint
static char specid[] = "@(#)dp_cplot.c	6.2  01/16/13 CSS";
#endif
/****************************************************************************
*
*   "spec" Release 6
*
*   Copyright (c) 1998,2012,2013
*   by Certified Scientific Software.
*   All rights reserved.
*   Copyrighted as an unpublished work.
*
****************************************************************************/

#include <stdio.h>
#include <user_pipe.h>

void
user_code() {
	char    *p;
	static  FILE    *pfd;

	if (pfd == NULL && (pfd = popen("cplot -s", "w")) == NULL) {
		printf("Can't popen() cplot ...\n");
		do_abort_return();
	}

	set_return_value(0.);
	if ((p = get_input_string()))
		if (fputs(p, pfd) == EOF)
			set_return_value(-1.);
	fflush(pfd);
}

/****************************************************************************

# The following macro provides a handy interface to the above ...

def cp_cmd(s) '{
	return(data_pipe("dp_cplot", sprintf("%s\n", s)))
}'
def cp '{
	local s, got_arg

	if ($# > 0) {
		got_arg = 1
		s = "$*"
	}
	if (got_arg == 0)
		print "\n\
C-PLOT will run silently, although error messages will appear.\n\
It\'s best to use the one-line form of commands, as prompts\n\
from C-PLOT for missing info will not appear.\n\
Type ^C or \"quit\" to return to spec.\n\
Type \"kill\" to make C-PLOT exit.\n\n"
	for (;;) {
		if (got_arg == 0)
			s = input(" C-PLOT> ")
		if (s == "q" || s == "quit")
			break
		if (s == "k" || s == "kill") {
			data_pipe("dp_cplot", "kill")
			break
		}
		cp_cmd(s)
		if (got_arg)
			break
	}
}'

****************************************************************************/
