#!/bin/sh

if [ "x$BASH_VERSION" != "x" ] ; then
    set -o nounset
fi

set -xe

###############################################################################

SRC_DIR="${SRC_DIR:-}"
test -z "$SRC_DIR" && SRC_DIR="`pwd`"

AC_VERSION="${AC_VERSION:-}"
AM_VERSION="${AM_VERSION:-}"

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
    1.1[1-8])
        ;;
    *)
        set +x
        echo
        echo "You must have automake 1.11...1.18 installed."
        echo "Install the appropriate package for your distribution,"
        echo "or get the source tarball at http://ftp.gnu.org/gnu/automake/"
        exit 1
    ;;
esac

###############################################################################

if echo "$SRC_DIR" | grep -E -q '[[:space:]]' ; then
    printf "\nThe source path \"%q\" contains whitespace characters.\nPlease fix it.\n" "$SRC_DIR"
    exit 1
fi

###############################################################################

(
    cd "$SRC_DIR"

    ACLOCAL_ARG="$ACLOCAL_ARG -I m4"
    test -d m4-sde && ACLOCAL_ARG="$ACLOCAL_ARG -I m4-sde"
    test -d m4-static && ACLOCAL_ARG="$ACLOCAL_ARG -I m4-static"

    if [ "x${ACLOCAL_DIR}" != "x" ]; then
        ACLOCAL_ARG="$ACLOCAL_ARG -I $ACLOCAL_DIR"
    fi

    if grep -q '^ACLOCAL_AMFLAGS' Makefile.am ; then
        sed -i 's/^ACLOCAL_AMFLAGS[ ]*=.*/ACLOCAL_AMFLAGS = '"$ACLOCAL_ARG"'/' Makefile.am
    else
        (
            cat Makefile.am
            echo ""
            echo "ACLOCAL_AMFLAGS = $ACLOCAL_ARG"
        ) > Makefile.am.tmp
        mv Makefile.am.tmp Makefile.am
    fi

    test -f aclocal.m4 && rm aclocal.m4
    test -d m4 && rm -r m4
    mkdir m4

    if grep -q "^AM_GNU_GETTEXT" ./configure.ac ; then
        $AUTOPOINT --version | head -1
        $AUTOPOINT --force
    fi

    if grep -q "^AM_GLIB_GNU_GETTEXT" ./configure.ac ; then
        $GLIB_GETTEXTIZE --version | head -1
        $GLIB_GETTEXTIZE --force --copy
    fi

    if grep -q "^GTK_DOC_CHECK" ./configure.ac ; then
        $GTKDOCIZE --version | head -1
        $GTKDOCIZE --copy
        set +x
        echo
        echo "=> If you are going to 'make dist', please add configure option --enable-gtk-doc."
        echo "=> Otherwise, API documents won't be correctly built by gtk-doc."
        echo
        set -x
    fi

    if grep -E -q "^(AC_PROG_INTLTOOL|IT_PROG_INTLTOOL)" ./configure.ac ; then
        $INTLTOOLIZE --version | head -1
        $INTLTOOLIZE --force --copy --automake
    fi

    # XXX: a workaround for gettext >= 0.24
    # https://gitlab.archlinux.org/archlinux/packaging/packages/dovecot/-/issues/4
    # https://gitlab.archlinux.org/archlinux/packaging/packages/gettext/-/issues/6
    prefix=/usr
    if test ! -f "m4/nls.m4" -a -f "$prefix/share/gettext/m4/nls.m4" ; then
        ln -s /usr/share/gettext/m4/nls.m4 m4
    fi

    $LIBTOOLIZE --version | head -1
    $ACLOCAL --version | head -1
    $AUTOHEADER --version | head -1
    $AUTOMAKE --version | head -1
    $AUTOCONF --version | head -1

    $LIBTOOLIZE --force --copy
    $ACLOCAL $ACLOCAL_ARG
    $AUTOHEADER --force
    $AUTOMAKE --add-missing --copy --include-deps
    $AUTOCONF

    rm -rf autom4te.cache
)

# -%- use-tabs: no; -%-
