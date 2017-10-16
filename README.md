# 2waytalkin
Two way socket communication (server and client) using BSD sockets.

Usage: 2wsc [-l] [-u] [hostname] port
* If -l is set, 2wsc will act as a server.
* If -u is set, 2wsc will attempt to connect/"listen" using UDP instead of TCP.
* Hostname is required when attempting to connect to a host, duh.
