#!/usr/bin/env bash
# jenkins build helper script for openbsc.  This is how we build on jenkins.osmocom.org
#
# environment variables:
# * WITH_MANUALS: build manual PDFs if set to "1"
# * PUBLISH: upload manuals after building if set to "1" (ignored without WITH_MANUALS = "1")
#

if ! [ -x "$(command -v osmo-build-dep.sh)" ]; then
	echo "Error: We need to have scripts/osmo-deps.sh from http://git.osmocom.org/osmo-ci/ in PATH !"
	exit 2
fi


set -ex

base="$PWD"
deps="$base/deps"
inst="$deps/install"
export deps inst

osmo-clean-workspace.sh

mkdir "$deps" || true

osmo-build-dep.sh libosmocore "" '--disable-doxygen --enable-gnutls'

verify_value_string_arrays_are_terminated.py $(find . -name "*.[hc]")

export PKG_CONFIG_PATH="$inst/lib/pkgconfig:$PKG_CONFIG_PATH"
export LD_LIBRARY_PATH="$inst/lib"
export PATH="$inst/bin:$PATH"

do_build() {
	configure_flags="$1"
	distcheck_flags="$2"
	set +x
	echo
	echo
	echo
	echo " =============================== osmo-bsc ==============================="
	echo
	set -x

	cd "$base"
	autoreconf --install --force
	./configure $configure_flags
	$MAKE $PARALLEL_MAKE
	LD_LIBRARY_PATH="$inst/lib" $MAKE check \
	  || cat-testlogs.sh
	LD_LIBRARY_PATH="$inst/lib" \
	  DISTCHECK_CONFIGURE_FLAGS="$distcheck_flags" \
	  $MAKE distcheck \
	  || cat-testlogs.sh
	$MAKE maintainer-clean
}

osmo-build-dep.sh libosmo-abis

# To build ipaccess-utils we just need libosmocore and libosmo-abis:
ipacess_utils_flags="--enable-sanitize --disable-external-tests --disable-vty-tests --disable-osmo-bsc --enable-ipaccess-utils --enable-werror $CONFIG"
do_build "$ipacess_utils_flags" "$ipacess_utils_flags"

osmo-build-dep.sh libosmo-netif
osmo-build-dep.sh libosmo-sccp
osmo-build-dep.sh osmo-mgw

# Additional configure options and depends
CONFIG=""
if [ "$WITH_MANUALS" = "1" ]; then
	osmo-build-dep.sh osmo-gsm-manuals
	CONFIG="--enable-manuals"
fi

if [ "$WITH_MANUALS" = "1" ] && [ "$PUBLISH" = "1" ]; then
	make -C "$base/doc/manuals" publish
fi

do_build "--enable-sanitize --enable-external-tests --enable-werror $CONFIG" "--enable-vty-tests --enable-external-tests --enable-werror $CONFIG"

osmo-clean-workspace.sh
