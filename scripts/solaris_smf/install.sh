#!/usr/bin/sh

# Creates a SMF service for ndd
# (C) senjan@atlas.cz 2016


/usr/sbin/svccfg validate ./ndd_svc.xml
if [ "$?" -ne 0 ];
then
	echo "ndd_svc.xml is not valid, cannot install it"
	exit 1
fi

cp ./svc-ndd /lib/svc/method && chmod 0755 /lib/svc/method/svc-ndd
if [ "$?" -ne 0 ];
then
	echo "cannot copy svc-ndd"
	exit 1
fi

echo "Importing manifest..."
/usr/sbin/svccfg import ./ndd_svc.xml
if [ "$?" -ne 0 ];
then
	echo "cannot import the manifest"
	exit 1
fi

echo "Enabling service..."
/usr/sbin/svcadm enable svc:/site/ndd:default
sleep 5
/usr/bin/svcs -xv svc:/site/ndd:default
