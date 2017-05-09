/****************************************************************************
*   @(#)user_pipe.h	6.1  07/29/12 CSS
*
*   "spec" Release 6
*
*   Copyright (c) 1994,1995,1998,2009
*   by Certified Scientific Software.
*   All rights reserved.
*   Copyrighted as an unpublished work.
*
****************************************************************************/


#if __STDC__

char    *get_input_string(void);
int     get_group_npts(void);
int     get_group_number(void);
int     get_group_width(void);
int     get_input_data(double *x, int pts, int wid);
int     get_input_elem(double *x, int pts, int el);
int     get_return_group_number(void);
int     set_return_elem(double *x, int pts, int el, int last);
int     set_return_string(char *s);
void    do_abort_return(void);
void    do_error_return(void);
void    do_quit_return(void);
void    set_return_data(double *x, int pts, int wid, int last);
void    set_return_value(double v);

#else

char    *get_input_string();
int     get_group_npts();
int     get_group_number();
int     get_group_width();
int     get_input_data();
int     get_input_elem();
int     get_return_group_number();
int     set_return_elem();
int     set_return_string();
void    do_abort_return();
void    do_error_return();
void    do_quit_return();
void    set_return_data();
void    set_return_value();

#endif
