#!/usr/bin/sh

# Removes a SMF service for ndd
# (C) senjan@atlas.cz 2016

NDD_SVC=svc:/site/ndd:default

echo "Disabling service..."
/usr/sbin/svcadm disable $NDD_SVC
/usr/bin/svcs -H $NDD_SVC
sleep 5
echo "Deleting service..."
/usr/sbin/svccfg delete -f $NDD_SVC

