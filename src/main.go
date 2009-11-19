package main

import(
	"fmt";
	"flag";
	"os";
	"syscall";
	"log";
	. "./msgqueue";
	. "./server";
)

func daemonize(server *Server) {
	pid, err := os.ForkExec("lineupd", []string{}, []string{}, "/var/run", []*os.File{});
	if err != nil {
		server.Logger().Logf("couldn't fork: %s\n", err);
		os.Exit(1);
	}
	server.Logger().Logf("%d->%d\n", os.Getpid(), pid);
	if (pid < 0) {
		os.Exit(1);
	}
	if (pid > 0) {
		os.Exit(0);
	}
	syscall.Setsid();
	for i := 0; i < 3; i++ {
		syscall.Close(i);
	}
	for i := 0; i < 3; i++ {
		syscall.Open("/dev/null", os.O_RDWR, 0644);
	}
	syscall.Umask(027);
	syscall.Chdir("/var/run");
}

func makePidfile(server *Server, filename string) {
	fp, err := os.Open(fmt.Sprintf("%s", filename), os.O_WRONLY|os.O_TRUNC|os.O_CREATE, 0666);
	if err != nil {
		server.Logger().Logf("couldn't open pidfile `%s': %s\n", filename, err);
		os.Exit(1);
	}
	_, err = fp.WriteString(fmt.Sprintf("%d", os.Getpid()));
	if err != nil {
		server.Logger().Logf("couldn't write to pidfile `%s': %s\n", filename, err);
		os.Exit(1);
	}
	fp.Close();
}

/* a ternary operator would make this function seem silly :) */
func getLogger(logfile string, daemonized bool) (logger *log.Logger) {
	logfp, err := os.Open(fmt.Sprintf("%s", logfile), os.O_WRONLY|os.O_APPEND|os.O_CREATE, 0666);
	if err != nil {
		fmt.Fprintf(os.Stderr, "couldn't open logfile `%s': %s\n", logfile, err);
		os.Exit(1);
	}
	if daemonized {
		logger = log.New(logfp, nil, "", log.Ldate|log.Ltime);
	}
	else {
		logger = log.New(logfp, os.Stdout, "", log.Ldate|log.Ltime);
	}
	return;
}

func main() {
	var port *int = flag.Int("port", 9876, "port to run server on");
	var daemon *bool = flag.Bool("daemonize", false, "whether or not to daemonize process");
	var timeout *int = flag.Int("timeout", 30, "number of seconds to wait disconnect inactive clients");
	var logfile *string = flag.String("logfile", "lineupd.log", "filename of the log file to write to");
	var pidfile *string = flag.String("pidfile", "lineupd.pid", "filename of the pid file to use");
	flag.Parse();
	queue := NewMsgQueue(10);
	server := &Server{*port, *timeout, queue, getLogger(*logfile, *daemon), 0, 0};
	if *daemon {
		daemonize(server);
	}
	makePidfile(server, *pidfile);
	server.Serve();
}
