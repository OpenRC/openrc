#!/bin/bash
# Copyright (c) 2007-2018 The OpenRC Authors.
# See the Authors file at the top-level directory of this distribution and
# https://github.com/OpenRC/openrc/blob/HEAD/AUTHORS
#
# This file is part of OpenRC. It is subject to the license terms in
# the LICENSE file found in the top-level directory of this
# distribution and at https://github.com/OpenRC/openrc/blob/HEAD/LICENSE
# This file may not be copied, modified, propagated, or distributed
# except according to the terms contained in the LICENSE file.

set -e
set -u
set -x

# These are steps to run on TravisCI under a containerized Ubuntu system.
# See $TOP/.travis.yml for more info about the TravisCI setup.

# Run shellcheck, but don't fail (yet):
shellcheck --version
for shellscript in $(git ls-files); do
	case $shellscript in
		init.d/*.in) ;;
		sh/*.in) ;;
		sh/*.sh) ;;
		support/init.d.examples/*.in) ;;
		support/openvpn/*.sh) ;;
		support/sysvinit/halt.sh) ;;
		test/*.sh) ;;
		*) continue ;;
	esac
	echo "Checking ${shellscript} with shellcheck:"
	shellcheck -s sh "${shellscript}" || true
done

cpus=$(getconf _NPROCESSORS_CONF || echo 1)
# make on TravisCI doesn't support -O yet
make -j"${cpus}"

make test
