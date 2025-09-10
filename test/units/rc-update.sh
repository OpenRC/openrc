#!/bin/sh

set -ex

. ${1?}
rc_update=${2?}

rc_update() {
	"$rc_update" --root "$root" "$@" 2>&1
}

in_runlevel() {
	test "$(readlink "$root/etc/runlevels/${1?}/${2?}")" = "$root/etc/init.d/$2"
}

stacked() {
	test -d "$root/etc/runlevels/${1?}/${2?}"
}

# non-existent
( ! rc_update add nya )
( ! rc_update -s add nya )
( ! rc_update del nya )
( ! rc_update -s del nya )

cat > "$root/etc/init.d/nya" <<-EOF
#!/sbin/openrc-run
start() {
	:;
}
EOF

# non-executable
( ! rc_update add nya )
( ! rc_update -s add nya )

chmod +x "$root/etc/init.d/nya"

# implicit runlevel
rc_update add nya
in_runlevel default nya

rc_update del nya
( ! in_runlevel default nya)
# not in the runlevel
( ! rc_update del nya )

# explicit runlevel
rc_update add nya default
in_runlevel default nya
# not in the specified runlevel
( ! rc_update del nya boop )

# stacking
rc_update -s add boop
stacked default boop

rc_update -s del boop
( ! stacked boop nya )

rc_update -s add boop default
stacked default boop

rc_update -s del boop
( ! stacked boop default )
( ! rc_update -s del boop )

# show
rc_update add nya
rc_update | grep -qE "^\s*nya"
rc_update show | grep -qE "^\s*nya"
rc_update show default | grep -qE "^\s*nya"
rc_update del nya
( ! rc_update show | grep -qE "^\s*nya" )
( ! rc_update show default | grep -qE "^\s*nya" )

exit 0
