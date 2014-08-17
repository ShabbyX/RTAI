#!/bin/sh

srcdir=`dirname "$0"`
test -z "$srcdir" && srcdir=.

ORIGDIR=`pwd`
cd "$srcdir"

autoreconf -v --install || exit 1
cd $ORIGDIR || exit $?

# Do a temporary configure to generate initial makefiles
"$srcdir"/configure --with-linux-dir="/lib/modules/$(uname -r)/build" "$@"
