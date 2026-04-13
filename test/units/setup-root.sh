root=$(mktemp -d --tmpdir rc.XXXXXX)
#trap "rm -r $root" EXIT

sysdir="$root/etc"
export RC_LIBEXECDIR="$BUILD_ROOT"
export RC_PATH="$sysdir"
export RC_SVCDIR="$root/run/openrc"
mkdir -p "$RC_SVCDIR"
echo "default" > "$RC_SVCDIR/softlevel"

for dir in init.d conf.d runlevels; do
	mkdir -p "$sysdir/$dir"
done
for dir in sysinit boot default boop shutdown; do
	mkdir -p "$sysdir/runlevels/$dir"
done

for dir in daemons exclusive failed hotplugged inactive init.d \
		options scheduled started starting stopping tmp wasinactive; do
	mkdir -p "$root/run/openrc/$dir"
done

setup_path() {
	local IFS=:
	export PATH="$PATH:$*"
}

setup_path "$BUILD_ROOT"/src/*
