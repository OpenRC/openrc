root=$(mktemp -d --tmpdir rc.XXXXXX)
#trap "rm -r $root" EXIT

for dir in init.d conf.d runlevels; do
	mkdir -p "$root/etc/$dir"
done
for dir in sysinit boot default boop shutdown; do
	mkdir -p "$root/etc/runlevels/$dir"
done
for dir in daemons exclusive failed hotplugged inactive init.d \
		options scheduled started starting stopping tmp wasinactive; do
	mkdir -p "$root/run/openrc/$dir"
done
mkdir -p "$root/run/openrc"
echo "default" > "$root/run/openrc/softlevel"
