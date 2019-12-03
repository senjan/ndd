# ndd - Network Disk Daemon

ndd is an user-space implementation of ND(4p) protocol, used by early SUN Microsystems' diskless workstations to access data on server over network.

## About

The ND protocol was most likely developed within Sun Microsystems Inc. in early 80s and it was later completely replaced by NFS in SunOS 4.x (around 1989). The protocol is mentioned in some books and papers from 80s but its internals are described only in the man pages distributed with SunOS; this work is based on the version distributed with SunOS 3.5.

The ND protocol is very simple: client encapsulates block I/O request into one or multiple IP datagrams and transmit them to the server. The server performs the I/O and sends the reply back, to the client. Each network "disk" on the server is implemented as a dedicated raw disk slice. The protocol does not provide any kind of access control or support for concurrent access: each client is supposed to modify only its own data; shared data needs to be accessed read-only.

My implementation uses file(s) instead of disk slice(s) and can run either as a daemon or a foreground process. It was originally developed on Solaris 11, but it should basically run on any modern UNIX-like system which provides adequate support for SOCK_RAW/IPPROTO_IP sockets. The ndd was implemented from scratch, it does not contain any source code produced created by Sun Microsystems Inc.

## Getting Started

### Prerequisites

Obtain and install the [iniparser](https://github.com/ndevilla/iniparser)

### Installing

ndd uses GNU Autotools:
```
./bootstrap.sh
./configure
make
sudo make install
```

See the wiki page for more details on how to configure and use the ndd.

## Authors

Jan Senolt [senjan](https://github.com/senjan)

## License

This project is licensed under the MIT License - see the [COPYING](COPYING) file for details
