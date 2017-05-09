/****************************************************************************
*   @(#)pipe_test.c	6.1  07/29/12 CSS
*
*   "spec" Release 6
*
*   Copyright (c) 1994,1995,2005,2009
*   by Certified Scientific Software.
*   All rights reserved.
*   Copyrighted as an unpublished work.
*
****************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <user_pipe.h>

/*
*  Here static space for expected data is declared.  One could
*  alternatively dynamically allocate data space using malloc().
*  The
*
*       get_group_npts();
*       get_group_width();
*
*  functions return the size of the incoming data group.
*/

static  double  x[1024][2];     /* An array for the example below */
static  double  y[1024][2];     /* Another array for the example */

/*
*  user_code() is called by the overhead routine that communicates with
*  spec.  "argc" and "argv" are formed from the second argument
*  to the data_pipe() function from spec.  That string is broken into
*  space-delimited words.  "argc" is the number of words.  "argv" is an
*  array of pointers to each of those words.
*/
void
user_code(int argc, char **argv) {
	int     i, wid, pts;
	char    *p;

	/* One way of viewing the arguments: */
	printf("pid=%d %d arguments received\n", getpid(), argc);
	for (i = 0; i < argc; i++)
		printf("arg%d:  [%s]\n", i, argv[i]);

	/* Another way to step through the arguments: */
	while (argc--) {
		p = *argv++;
		/* check for -xyz style options */
		if (*p == '-') {
			if (!p[1]) {
				/* could do something on a bare "-" */
				continue;
			}
			/* switch on options */
			while (*++p) switch(*p) {
			  /*
			  *  This function can arrange that either a
			  *  string or number will be returned by
			  *  the data_pipe() function from spec.
			  */
			  case 's':
				/* Here we set a string to return to spec */
				set_return_string("this is the return string");
				break;
			  case 'n':
				/* Here we set a number to return to spec */
				set_return_value(666.);
				break;

			  /*
			  *  An abort return will cause spec to reset to
			  *  command level.
			  */
			  case 'x':
				do_abort_return();
				/*NOTREACHED*/
			  /*
			  *  An error return will cause data_pipe() to
			  *  return either a -1, or a number return
			  *  value, if set_return_value() is used.
			  */
			  default:
				set_return_value(-2.);
				do_error_return();
				/*NOTREACHED*/
			}
		} else {
			/* could do something else if no "-" */
			;
		}
	}

	/* Perhaps we would like to know how big the incoming data is ... */
	pts = get_group_npts();
	wid = get_group_width();
	printf("The incoming data has %d points with %d elements per point.\n",
								    pts, wid);

	/*
	*  Copy the input data group to our storage.
	*  Pass the maximum data points and elements wanted.
	*  If there are insufficient points or elements in the
	*  data group, only as many as exist will be copied.
	*/
	pts = get_input_data(x[0], 1024, 2);
	printf("Retrieved %d points.\n", pts);

	/*
	*  Do something interesting with the input data group.
	*/
	for (i = 0; i < 20; i++) {
		y[i][0] = x[i][0] + i;
		y[i][1] = x[i][1] + i * 4;
	}

	/*
	*  Have the output data returned to a spec data group, as
	*  specified by data_pipe().  Pass the number of points and
	*  elements we want passed.  data_pipe() will create a new
	*  group of the required size.  The last argument below sets
	*  the "last-accessed" point, used by some of the other data
	*  commands in spec, such as data_dump() and data_plot().
	*/
	set_return_data(y[0], 1024, 2, 20);
}
