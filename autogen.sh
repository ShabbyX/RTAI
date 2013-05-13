#! /bin/sh

srcdir=`dirname "$0"`
test -z "$srcdir" && srcdir=.

ORIGDIR=`pwd`
cd "$srcdir"

autoreconf -v --install || exit 1
cd $ORIGDIR || exit $?

echo -e "\nYou can now run make menuconfig"
echo -e "or at your option ./configure\n"
