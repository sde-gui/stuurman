#!/bin/sh

if [ "x$BASH_VERSION" != "x" ] ; then
    set -o nounset
fi

set -xe

###############################################################################

SRC_DIR="${SRC_DIR:-}"

AC_VERSION="${AC_VERSION:-}"
AM_VERSION="${AC_VERSION:-}"

ACLOCAL_ARG="${ACLOCAL_ARG:-}"
ACLOCAL_DIR="${ACLOCAL_DIR:-}"

export AUTOMAKE="${AUTOMAKE:-automake$AM_VERSION}"
export ACLOCAL="${ACLOCAL:-aclocal$AM_VERSION}"

export AUTOHEADER="${AUTOHEADER:-autoheader$AC_VERSION}"
export AUTOCONF="${AUTOCONF:-autoconf$AC_VERSION}"
export AUTORECONF="${AUTORECONF:-autoreconf$AC_VERSION}"

export AUTOPOINT="${AUTOPOINT:-autopoint}"
export LIBTOOLIZE="${LIBTOOLIZE:-libtoolize}"
export INTLTOOLIZE="${INTLTOOLIZE:-intltoolize}"
export GLIB_GETTEXTIZE="${GLIB_GETTEXTIZE:-glib-gettextize}"
export GTKDOCIZE="${GTKDOCIZE:-gtkdocize}"

###############################################################################

if [ "x`uname`" = xOpenBSD ] ; then
    V="`ls -1 /usr/local/bin/autoreconf-* | env LC_ALL=C sort | tail -n 1`"
    V="${V##*-}"
    export AUTOCONF_VERSION="${AUTOCONF_VERSION:-$V}"

    V="`ls -1 /usr/local/bin/automake-* | env LC_ALL=C sort | tail -n 1`"
    V="${V##*-}"
    export AUTOMAKE_VERSION="${AUTOMAKE_VERSION:-$V}"
fi

###############################################################################

AM_INSTALLED_VERSION=$($AUTOMAKE --version | sed -e '2,$ d' -e 's/.* \([0-9]*\.[0-9]*\).*/\1/')

case "$AM_INSTALLED_VERSION" in
    1.1[1-6])
        ;;
    *)
        set +x
        echo
        echo "You must have automake 1.11...1.16 installed."
        echo "Install the appropriate package for your distribution,"
        echo "or get the source tarball at http://ftp.gnu.org/gnu/automake/"
        exit 1
    ;;
esac

###############################################################################

if [ -z "$SRC_DIR" ] ; then
    SRC_DIR="`pwd`"
fi

if echo "$SRC_DIR" | grep -E -q '[[:space:]]' ; then
    printf "\nThe source path \"%q\" contains whitespace characters.\nPlease fix it.\n" "$SRC_DIR"
    exit 1
fi

###############################################################################

(
    cd "$SRC_DIR"

    if [ "x${ACLOCAL_DIR}" != "x" ]; then
        ACLOCAL_ARG="$ACLOCAL_ARG -I $ACLOCAL_DIR"
    fi

    test -f aclocal.m4 && rm aclocal.m4
    test -d m4 && rm -r m4
    mkdir m4

    if grep -q "^AM_GNU_GETTEXT" ./configure.ac ; then
        $AUTOPOINT --force
    fi

    if grep -q "^AM_GLIB_GNU_GETTEXT" ./configure.ac ; then
        $GLIB_GETTEXTIZE --force --copy
    fi

    if grep -q "^GTK_DOC_CHECK" ./configure.ac ; then
        $GTKDOCIZE --copy
        set +x
        echo
        echo "=> If you are going to 'make dist', please add configure option --enable-gtk-doc."
        echo "=> Otherwise, API documents won't be correctly built by gtk-doc."
        echo
        set -x
    fi

    if grep -E -q "^(AC_PROG_INTLTOOL|IT_PROG_INTLTOOL)" ./configure.ac ; then
        $INTLTOOLIZE --force --copy --automake
    fi

    $ACLOCAL $ACLOCAL_ARG
    $LIBTOOLIZE
    $AUTOHEADER --force
    $AUTOMAKE --add-missing --copy --include-deps
    $AUTOCONF

    rm -rf autom4te.cache
)

