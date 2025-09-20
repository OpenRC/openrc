#!/bin/sh

set -e

. "$SUBDIR/setup-root.sh"
rc_service=${1?}
openrc_run=${2?}
cp "$RC_LIBEXECDIR/sh/openrc-run.sh" "$root/run/openrc/openrc-run.sh"
cp "$RC_LIBEXECDIR/sh/gendepends.sh" "$root/run/openrc/gendepends.sh"
chmod +x "$root/run/openrc/openrc-run.sh"
chmod +x "$root/run/openrc/gendepends.sh"

rc_service() {
	$rc_service "$@" 2>&1
}

in_state() {
	state=${1?}
	shift
	for svc; do
		test "$(readlink "$RC_SVCDIR/$state/$svc")" = "$root/etc/init.d/$svc"
	done
}

mkservice() {
	service="$root/etc/init.d/$1"
	cat > "$service" <<-EOF
	#!$SOURCE_ROOT/${openrc_run}
	depend() {
		${2-:;}
	}
	start() {
		${3-sleep 0.1}
	}
	EOF
	chmod +x "$service"
}

mkservice nya
mkservice foo "need nya"
mkservice mew "want foo"

addrunlevel() {
	ln -s "$root/etc/init.d/${1?}" "$root/etc/runlevel/${2?}/$1"
}

rc_service nya start
in_state started nya

rc_service nya stop
( ! in_state started nya )

rc_service foo start
in_state started foo nya

rc_service foo stop
in_state started nya
( ! in_state started foo )

rc_service foo start
in_state started foo nya

rc_service nya stop
( ! in_state started foo nya )

rc_service mew start
in_state started foo nya mew

rc_service nya stop
rc_service mew stop

( ! in_state started foo nya mew )
