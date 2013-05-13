dnl ########################### -*- Mode: M4 -*- #######################
dnl Copyright (C) 98, 1999 Matthew D. Langston <langston@SLAC.Stanford.EDU>
dnl
dnl This macro is free software; you can redistribute it and/or modify it
dnl under the terms of the GNU General Public License as published by
dnl the Free Software Foundation; either version 2 of the License, or
dnl (at your option) any later version.
dnl
dnl This file is distributed in the hope that it will be useful, but
dnl WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
dnl General Public License for more details.
dnl
dnl You should have received a copy of the GNU General Public License
dnl along with this file; if not, write to:
dnl
dnl   Free Software Foundation, Inc.
dnl   Suite 330
dnl   59 Temple Place
dnl   Boston, MA 02111-1307, USA.
dnl ####################################################################
dnl @synopsis MDL_HAVE_OPENGL
dnl
dnl A very simple check for OpenGL headers and libraries
dnl
dnl @version 2013-05-12
dnl @author Alec Ari <neotheuser@ymail.com>
dnl @license GPLWithACException
dnl
AC_DEFUN([MDL_HAVE_OPENGL],
[
AC_REQUIRE([AC_PATH_X])
AC_REQUIRE([AC_PATH_XTRA])
AC_CHECK_HEADERS(GL/gl.h GL/glu.h,[],[AC_MSG_ERROR([Required OpenGL header missing.])])
AC_CHECK_LIB(GL, glBegin, [], [AC_MSG_ERROR([Required GL library missing.])])
])

AC_DEFUN([SC_PATH_EFLTK], [

dnl EFLTK dir passed to the command line overrides the Kconfig setting

AC_ARG_WITH(efltk, [  --with-efltk              directory containing EFLTK], with_efltk=${withval})

dnl Defaults to the Kconfig setting (CONFIG_RTAI_EFLTK_DIR) if unset

test x$with_efltk = x && with_efltk=$1

AC_MSG_CHECKING([for EFLTK])

AC_CACHE_VAL(ac_cv_efltk,[

    # First check to see if --with-efltk was specified.
    if test x"${with_efltk}" != x ; then
	if test -f "${with_efltk}/bin/efltk-config" ; then
	    ac_cv_efltk=`(cd ${with_efltk}; pwd)`
	else
	    AC_MSG_ERROR([${with_efltk}/bin directory doesn't contain efltk-config])
	fi
    fi

    # then check for a private EFLTK installation
    if test x"${ac_cv_efltk}" = x ; then
	for i in \
		../efltk \
		`ls -dr ../efltk* 2>/dev/null` \
		../../efltk \
		`ls -dr ../../efltk* 2>/dev/null` \
		../../../efltk \
		`ls -dr ../../../efltk* 2>/dev/null` ; do
	    if test -f "$i/bin/efltk-config" ; then
		ac_cv_efltk=`(cd $i; pwd)`
		break
	    fi
	done
    fi

    # check in a few common install locations
    if test x"${ac_cv_efltk}" = x ; then
	for i in ${prefix}/efltk* /usr/local/efltk* /usr/pkg/efltk* /usr \
		`ls -dr /usr/lib/efltk* 2>/dev/null` ; do
	    if test -f "$i/bin/efltk-config" ; then
		ac_cv_efltk=`(cd $i; pwd)`
		break
	    fi
	done
    fi

    # check in a few other private locations
    if test x"${ac_cv_efltk}" = x ; then
	for i in \
		${srcdir}/../efltk \
		`ls -dr ${srcdir}/../efltk* 2>/dev/null` ; do
	    if test -f "$i/bin/efltk-config" ; then
	    ac_cv_efltk=`(cd $i; pwd)`
	    break
	fi
	done
    fi
    ])

if test x"${ac_cv_efltk}" = x ; then
    EFLTK_DIR="# no EFLTK installation found"
    AC_MSG_ERROR(Can't find EFLTK-devel installation (missing bin/efltk-config?))
else
    EFLTK_DIR=${ac_cv_efltk}
    AC_MSG_RESULT(found $EFLTK_DIR)
fi

])

AC_DEFUN([SC_PATH_COMEDI], [

dnl COMEDI dir passed to the command line overrides the Kconfig setting

AC_ARG_WITH(comedi, [  --with-comedi              directory containing COMEDI], with_comedi=${withval})

dnl Defaults to the Kconfig setting (CONFIG_RTAI_COMEDI_DIR) if unset

test x$with_comedi = x && with_comedi=$1

AC_MSG_CHECKING([for COMEDI])

AC_CACHE_VAL(ac_cv_comedi,[

    # First check to see if --with-comedi was specified.
    if test x"${with_comedi}" != x ; then
	if test -f "${with_comedi}/include/linux/comedilib.h" ; then
	    ac_cv_comedi=`(cd ${with_comedi}; pwd)`
	else
	    AC_MSG_ERROR([${with_comedi}/include directory doesn't contain linux/comedilib.h])
	fi
    fi

    # then check for a private COMEDI installation
    if test x"${ac_cv_comedi}" = x ; then
	for i in \
		../comedi \
		`ls -dr ../comedi* 2>/dev/null` \
		../../comedi \
		`ls -dr ../../comedi* 2>/dev/null` \
		../../../comedi \
		`ls -dr ../../../comedi* 2>/dev/null` ; do
	    if test -f "$i/include/linux/comedilib.h" ; then
		ac_cv_comedi=`(cd $i; pwd)`
		break
	    fi
	done
    fi

    # check in a few common install locations
    if test x"${ac_cv_comedi}" = x ; then
	for i in ${prefix}/comedi* /usr/local/comedi* /usr/pkg/comedi* /usr \
		`ls -dr /usr/lib/comedi* 2>/dev/null` ; do
	    if test -f "$i/include/linux/comedilib.h" ; then
		ac_cv_comedi=`(cd $i; pwd)`
		break
	    fi
	done
    fi

    # check in a few other private locations
    if test x"${ac_cv_comedi}" = x ; then
	for i in \
		${srcdir}/../comedi \
		`ls -dr ${srcdir}/../comedi* 2>/dev/null` ; do
	    if test -f "$i/include/linux/comedilib.h" ; then
	    ac_cv_comedi=`(cd $i; pwd)`
	    break
	fi
	done
    fi
    ])

if test x"${ac_cv_comedi}" = x ; then
    COMEDI_DIR="# no COMEDI installation found"
    AC_MSG_ERROR(Can't find COMEDI installation ( missing linux/comedilib.h ) )
else
    COMEDI_DIR=${ac_cv_comedi}
    AC_MSG_RESULT(found $COMEDI_DIR)
fi

])
