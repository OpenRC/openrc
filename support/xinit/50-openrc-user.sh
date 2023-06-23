#!/bin/sh
# Copyright (c) 2007-2023 The OpenRC Authors.
# See the Authors file at the top-level directory of this distribution and
# https://github.com/OpenRC/openrc/blob/HEAD/AUTHORS
#
# This file is part of OpenRC. It is subject to the license terms in
# the LICENSE file found in the top-level directory of this
# distribution and at https://github.com/OpenRC/openrc/blob/HEAD/LICENSE
# This file may not be copied, modified, propagated, or distributed
#    except according to the terms contained in the LICENSE file.

# Graphical applications launched by dbus need those variables in an X
# environment.
if  command -v dbus-update-activation-environment >/dev/null 2>&1 &&
	rc-service --user dbus status >/dev/null 2>&1
then
	dbus-update-activation-environment DISPLAY XAUTHORITY
fi
