dnl $Id$
dnl config.m4 for extension ahttp

dnl Comments in this file start with the string 'dnl'.
dnl Remove where necessary. This file will not work
dnl without editing.

dnl If your extension references something external, use with:

PHP_ARG_ENABLE(ahttp, whether to enable test support,
[  --enable-ahttp           Enable ahttp support])

PHP_ARG_WITH(libevent, for the location of libevent,
[  --with-libevent[=DIR]             Include libevent support])


  if test "$PHP_LIBEVENT" != "no"; then

	SEARCH_PATH="/usr /usr/local"
	SEARCH_FOR="/include/event.h"
	
	SEARCH_PATH="/usr"     # you might want to change this
	SEARCH_FOR="/include/event.h"  # you most likely want to change this
 	if test -r $PHP_LIBEVENT/$SEARCH_FOR; then # path given as parameter
    	LIBEVENT_DIR=$PHP_LIBEVENT
  	else # search default path list
    	AC_MSG_CHECKING([for libevent files in default path])
     	for i in $SEARCH_PATH ; do
       		if test -r $i in $SEARCH_FOR; then
         	LIBEVENT_DIR=$PHP_LIBEVENT
         	AC_MSG_RESULT(found in $i)
       fi
     done
   fi


    if test -z "$LIBEVENT_DIR"; then
        AC_MSG_RESULT([not found])
    	AC_MSG_ERROR([Please reinstall the libevent distribution])
    fi
    
    PHP_ADD_INCLUDE($LIBEVENT_DIR/include)
    
 	LIBNAME=event
 	LIBSYMBOL=event_base_new
 	
 	if test "x$PHP_LIBDIR" = "x"; then
    	PHP_LIBDIR=lib
  	fi

 	PHP_CHECK_LIBRARY($LIBNAME,$LIBSYMBOL,
   	[
    	PHP_ADD_LIBRARY_WITH_PATH($LIBNAME, $LIBEVENT_DIR/$PHP_LIBDIR, AHTTP_SHARED_LIBADD)
     	AC_DEFINE(HAVE_LIBEVENT,1,[ ])
   	],[
     	AC_MSG_ERROR([wrong libevent version {1.4.+ is required} or lib not found])
   	],[
     	-L$LIBEVENT_DIR/$PHP_LIBDIR
   	])
  
	PHP_SUBST(AHTTP_SHARED_LIBADD)

	else
    	AC_MSG_RESULT([If configure fails try --with-libevent=<DIR> ])
  	fi

if test "$PHP_AHTTP" != "no"; then

  PHP_NEW_EXTENSION(ahttp, ahttp.c, $ext_shared,, -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1)
fi
