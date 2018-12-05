#!/bin/bash
# Copyright (c) 2007-2018 The OpenRC Authors.
# See the Authors file at the top-level directory of this distribution and
# https://github.com/OpenRC/openrc/blob/master/AUTHORS
#
# This file is part of OpenRC. It is subject to the license terms in
# the LICENSE file found in the top-level directory of this
# distribution and at https://github.com/OpenRC/openrc/blob/master/LICENSE
# This file may not be copied, modified, propagated, or distributed
# except according to the terms contained in the LICENSE file.

set -e
set -u
set -x

# These are steps to run on Cirrus CI under a jailed FreeBSD system.
# See $TOP/.cirrus.yml for more info about the Cirrus CI setup.

cpus=$(getconf NPROCESSORS_CONF || echo 1)
gmake -j"${cpus}" -O
gmake test
