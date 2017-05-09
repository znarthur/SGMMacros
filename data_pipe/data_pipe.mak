#   @(#)data_pipe.mak	6.1  07/29/12 CSS
#
#   "spec" Release 6
#   Copyrighted as an unpublished work.
#   Copyright (c) 1994,1995,2007 Certified Scientific Software
#   All rights reserved.
#

SPECD   = /home/sgm/lib/spec.d
SHELL   = /bin/sh

PDIR    = data_pipe

OPTIM   = -O
GFLAG   =
MFLAG   = -m64
LIBS    = -lm
UOBJ    =

CFLAGS  = ${OPTIM} ${GFLAG} ${MFLAG} -I${SPECD}/${PDIR}
LDFLAGS = ${MFLAG} ${GFLAG}

target  =

it: check ${target}

check:
	@sh -c "if test -z \"${target}\" ; then \
		echo \
		  Invoke as \"make target=something [UOBJ=... LIBS=...] \" ; \
		exit 1 ; fi ; exit 0"

${target}: ${SPECD}/${PDIR}/data_pipe.o ${target}.o ${UOBJ}
	${CC} ${LDFLAGS} ${GFLAG} ${SPECD}/${PDIR}/data_pipe.o \
		${target}.o ${UOBJ} -o ${target} ${LIBS}

.c.o:
	${CC} ${CFLAGS} -c $*.c
