#!/bin/bash --login
SOURCE="${BASH_SOURCE[0]}"
DIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"

os=$( uname )
[[ $os != 'Darwin' ]] && echo "For macOS only" && exit 1

# See https://dansketcher.com/2021/03/30/shmmax-error-on-big-sur/

shmmax=$( sysctl -A kern.sysv.shmmax | awk '{ print $2 }' )
echo "Current kern.sysv.shmmax=$shmmax"
if [[ $shmmax -gt 268435455 ]] ;then
	echo "shmmax is big enough"
else
	# The short term (until reboot) solution:
	echo sudo sysctl -w kern.sysv.shmmax=268435456
	sudo sysctl -w kern.sysv.shmmax=268435456
fi

shmall=$( sysctl -A kern.sysv.shmall | awk '{ print $2 }' )
echo "Current kern.sysv.shmall=$shmall"
if [[ $shmall -gt 65535 ]] ;then
	echo "shmall is big enough"
else
	# The short term (until reboot) solution:
	echo sudo sysctl -w kern.sysv.shmall=65536
	sudo sysctl -w kern.sysv.shmall=65536
fi

# A more permanent switch, since Catalina
plist='/Library/LaunchDaemons/chg_shmmax.plist'
[[ -f $plist ]] && echo "$plist exists already" && exit 0
echo sudo cp -v $DIR/patch/macos_chg_shmmax.plist chg_shmmax.plist
sudo cp -v $DIR/patch/macos_chg_shmmax.plist /Library/LaunchDaemons/chg_shmmax.plist
echo sudo launchctl load /Library/LaunchDaemons/chg_shmmax.plist
sudo launchctl load /Library/LaunchDaemons/chg_shmmax.plist
