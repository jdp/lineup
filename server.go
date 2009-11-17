package server

import(
	"os";
	"fmt";
	"log";
	"net";
	"regexp";
	"strings";
	//"bytes";
	"bufio";
	. "./msgqueue"
)

/*
 * Represents a connected client. Maybe not the best name.
 */
type Request struct {
	conn net.Conn; // Over-the-wire connection
}

/*
 * Represents the server.
 */
type Server struct {
	port int; // Server port
	timeout int;
	mq *MsgQueue; // Message queue
	connections int; // Total current connections
	totalConnections int; // Total lifetime connections
}

/*
 * Utility function to convert a slice of bytes, each byte being a
 * digit, into a single integer.
 */
func readInt(b []byte) int {
	i, x := 0, 0;
	for ; i < len(b) && (b[i] >= '0' && b[i] <= '9'); i++ {
		x = x*10 + int(b[i])-'0'
	}
	return x;
}

/*
 * Used to enforce terminating CRLF in client requests.
 */
func handleCrlf(req *Request, src *bufio.Reader) (err os.Error) {
	crlf := make([]byte, 2);
	n, err := src.Read(crlf);
	if err != nil {
		return;
	}
	if n != 2 {
		err = os.NewError("missing 2 trailing bytes");
		return;
	}
	if crlf[0] != '\r' || crlf[1] != '\n' {
		err = os.NewError(fmt.Sprintf("expecting [13 10], got %v", crlf));
		return;
	}
	return;
}

/*
 * Responds to the GIVE command
 *
 * The GIVE command accepts data from the client in this format:
 *   GIVE <priority> <message-size>\r\n
 *   <message-data>\r\n
 * Where <priority> is the priority of the message, <message-size> is
 * the length of the message in bytes, and <message-data> is a chunk
 * of bytes with a length equal to <message-size>. On success, the
 * server responds with OK in inline format and the message is added
 * to the queue. Otherwise, an error message is sent in Error format.
 *
 * Successful response:
 *   +OK\r\n
 *
 * Unsuccessful response:
 *   -<error-message>\r\n
 */
func (s *Server) handleGiveCmd(req *Request, src *bufio.Reader, priority int, size int) (err os.Error) {
	c, err := src.ReadByte();
	if err != nil {
		return;
	}
	if c != '\n' {
		err = os.NewError(fmt.Sprintf("expected `\\n', got `%c'", c));
		return;
	}
	message := make([]byte, size);
	nn, err := src.Read(message);
	if err != nil {
		return;
	}
	fmt.Printf("handleGiveCmd: read %d bytes\n", nn);
	if len(message) < size {
		err = os.NewError(fmt.Sprintf("message not fully read: %d of %d bytes read", len(message), size));
		return;
	}
	if err := handleCrlf(req, src); err != nil {
		return err;
	}
	/* Here is where the message would be inserted into the queue */
	m := &Message{priority,message};
	s.mq.Give(m);
	req.conn.Write(strings.Bytes("+OK\r\n"));
	return;
}

/*
 * Responds to the TAKE command.
 *
 * If there is anything in the queue, it removes the message with the
 * highest priority and sends it to the client in Message format. If
 * any errors occur, they are sent back to the client in Error format.
 *
 * Successful response:
 *   $<message-length>\r\n
 *   <message-data>\r\n
 *
 * Unsuccessful response:
 *  -<error-message>\r\n
 */
func (s *Server) handleTakeCmd(req *Request, src *bufio.Reader) (err os.Error) {
	c, err := src.ReadByte();
	if err != nil {
		return;
	}
	if c != '\n' {
		err = os.NewError(fmt.Sprintf("expected `\\n', got `%c'", c));
		return;
	}
	if s.mq.Len() == 0 {
		req.conn.Write(strings.Bytes("-NO_MESSAGES\r\n"));
	}
	else {
		m := s.mq.Take();
		req.conn.Write(strings.Bytes(fmt.Sprintf("$%d\r\n", len(m.Bytes()))));
		req.conn.Write(m.Bytes());
		req.conn.Write([]byte{'\r','\n'});
	}
	return;
}

/*
 * This function is spawned as a goroutine for each request.
 * It reacts to commands coming from clients and handles them accordingly.
 */
func (s *Server) handle(req *Request) {
	
	giveCmd, _ := regexp.Compile("^GIVE ([0-9]+) ([0-9]+)\r");
	takeCmd, _ := regexp.Compile("^TAKE\r");
	pingCmd, _ := regexp.Compile("^PING\r");
	exitCmd, _ := regexp.Compile("^EXIT\r");
	disconnected := false;
	
	//req.conn.SetReadTimeout(1e8);
	
	for {
		buf := bufio.NewReader(req.conn);
		line, err := buf.ReadBytes('\r');
		if err != nil {
			switch {
				case err == os.EOF:
					disconnected = true;
				default:
					log.Stderrf("read error: %v\n", err);
					break;
			}
		}
		if len(line) > 0 {
			switch {
				// Handle the GIVE command
				case giveCmd.Match(line):
					matches := giveCmd.MatchSlices(line);
					priority, size := readInt(matches[1]), readInt(matches[2]);
					if err := s.handleGiveCmd(req, buf, priority, size); err != nil {
						log.Stderrf("GIVE: failed (priority %d; size %d) from %s: %s\n", priority, size, req.conn.RemoteAddr(), err);
					}
					else {
						log.Stdoutf("GIVE: successful (priority %d; size %d) from %s; size: %d\n", priority, size, req.conn.RemoteAddr(), s.mq.Len());
					}
				
				// Handle the TAKE command
				case takeCmd.Match(line):
					if err := s.handleTakeCmd(req, buf); err != nil {
						log.Stderrf("TAKE: failed from %s: %s\n", req.conn.RemoteAddr(), err);
					}
					else {
						log.Stdoutf("TAKE: successful from %s; size %d\n", req.conn.RemoteAddr(), s.mq.Len());
					}
				
				// Handle the PING command
				case pingCmd.Match(line):
					req.conn.Write(strings.Bytes("+PONG\r\n"));
					log.Stdoutf("PING: from %s\n", req.conn.RemoteAddr());

				// Handle the EXIT command
				case exitCmd.Match(line):
					disconnected = true;
					log.Stdoutf("EXIT: from %s\n", req.conn.RemoteAddr());
				
				// No matching command to handle
				default:
					req.conn.Write(strings.Bytes("-INVALID_COMMAND\r\n"));
					log.Stderrf("invalid command from %s\n", req.conn.RemoteAddr());
			}
			fmt.Printf("bytes left: %d\n", buf.Buffered());
		}
		fmt.Printf("waiting on nothing...\n");
		
		if disconnected {
			log.Stdoutf("disconnected: %s\n", req.conn.RemoteAddr());
			req.conn.Close();
			s.connections--;
			break;
		}
	}
}

/*
 * The actual server loop. Just chills out and accepts connections
 * until it is sent a quit message.
 */
func (s *Server) Serve() {
	listener, err := net.Listen("tcp", fmt.Sprintf(":%d", s.port));
	if err != nil {
		log.Exitf("could not start server: %s\n", err);
	}
	log.Stdoutf("waiting for connections on %s\n", listener.Addr());
	for {
		conn, err := listener.Accept();
		if err != nil {
			log.Exitf("couldn't accept: %s\n", err);
		}
		log.Stdoutf("connected: %s\n", conn.RemoteAddr());
		s.connections++;
		s.totalConnections++;
		go s.handle(&Request{conn});
	}
}

