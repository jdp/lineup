package main

import(
	//"fmt";
	"flag";
	. "./msgqueue";
	. "./server";
)

func main() {
	var port *int = flag.Int("port", 9876, "port to run server on");
	var daemon *bool = flag.Bool("daemonize", false, "whether or not to daemonize process");
	var timeout *int = flag.Int("timeout", 30, "number of seconds to wait disconnect inactive clients");
	flag.Parse();
	if *daemon {
		// daemonize
	}
	queue := NewMsgQueue(10);
	server := &Server{*port, *timeout, queue, 0, 0};
	server.Serve();
}
