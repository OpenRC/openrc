#!/bin/sh

set -ex

. ${1?}
rc_service=${2?}
openrc_run=${3?}
cp "${4?}" "$root/run/openrc/openrc-run.sh"
cp "${5?}" "$root/run/openrc/gendepends.sh"
chmod +x "$root/run/openrc/openrc-run.sh"
chmod +x "$root/run/openrc/gendepends.sh"

rc_service() {
	$rc_service --root "$root" "$@" 2>&1
}

in_state() {
	state=${1?}
	shift
	for svc; do
		test "$(readlink "$root/run/openrc/$state/$svc")" = "$root/etc/init.d/$svc"
	done
}

cat > "$root/etc/init.d/foo" <<-EOF
#!${openrc_run}
depend() {
	need nya
}
start() {
	:;
}
EOF

cat > "$root/etc/init.d/nya" <<-EOF
#!${openrc_run}
start() {
	:;
}
EOF

chmod +x "$root/etc/init.d/"*

rc_service nya start
in_state started nya

rc_service nya stop
( ! in_state started nya )

#rc_service foo start
#in_state started foo nya
