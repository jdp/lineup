package server

import(
	"os";
	"fmt";
	"log";
	"net";
	"regexp";
	"strings";
	"bytes";
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
func handleCrlf(req *Request) (err os.Error) {
	crlf := make([]byte, 2);
	n, err := req.conn.Read(crlf);
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
func (s *Server) handleGiveCmd(req *Request, priority int, size int) (err os.Error) {
	message := make([]byte, 0);
	totalRead := 0;
	for {
		tmp := make([]byte, 1024);
		if len(tmp) + totalRead > size {
			tmp = make([]byte, size - totalRead);
		}
		read, err := req.conn.Read(tmp); 
		if err != nil {
			return;
		}
		if len(tmp) != read {
			log.Stderrf("fucker! %d:%d\n", len(tmp), read);
			tmp = tmp[0:read];
		}
		message = bytes.Add(message, tmp);
		totalRead += read;
		if totalRead >= size {
			break;
		}
	}
	if len(message) < size {
		err = os.NewError(fmt.Sprintf("message not fully read: %d of %d bytes read", len(message), size));
		return;
	}
	if err := handleCrlf(req); err != nil {
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
func (s *Server) handleTakeCmd(req *Request) (err os.Error) {
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
	
	giveCmd, _ := regexp.Compile("^GIVE ([0-9]+) ([0-9]+)\r\n");
	takeCmd, _ := regexp.Compile("^TAKE\r\n");
	pingCmd, _ := regexp.Compile("^PING\r\n");
	exitCmd, _ := regexp.Compile("^EXIT\r\n");
	disconnected := false;
	
	req.conn.SetTimeout(int64(s.timeout) * 1e9);

	for {
	
		buf := make([]byte, 512);
		n, err := req.conn.Read(buf);
		switch err {
			case nil:
				// Do nothing
			case os.EOF:
				disconnected = true;
			default:
				log.Stdoutf("read error from %s: %s\n", req.conn.RemoteAddr(), err);
				disconnected = true;
		}
		
		if n > 0 {
			switch {
				// Handle the GIVE command
				case giveCmd.Match(buf):
					matches := giveCmd.MatchSlices(buf);
					priority, size := readInt(matches[1]), readInt(matches[2]);
					if err := s.handleGiveCmd(req, priority, size); err != nil {
						log.Stderrf("GIVE: failed from %s: %s\n", req.conn.RemoteAddr(), err);
					}
					else {
						log.Stdoutf("GIVE: successful (priority %d; size %d) from %s; size: %d\n", priority, size, req.conn.RemoteAddr(), s.mq.Len());
					}
				
				// Handle the TAKE command
				case takeCmd.Match(buf):
					if err := s.handleTakeCmd(req); err != nil {
						log.Stderrf("TAKE: failed from %s: %s\n", req.conn.RemoteAddr(), err);
					}
					else {
						log.Stdoutf("TAKE: successful from %s; size %d\n", req.conn.RemoteAddr(), s.mq.Len());
					}
				
				// Handle the PING command
				case pingCmd.Match(buf):
					req.conn.Write(strings.Bytes("+PONG\r\n"));
					log.Stdoutf("PING: from %s\n", req.conn.RemoteAddr());

				// Handle the EXIT command
				case exitCmd.Match(buf):
					disconnected = true;
					log.Stdoutf("EXIT: from %s\n", req.conn.RemoteAddr());
				
				// No matching command to handle
				default:
					req.conn.Write(strings.Bytes("-INVALID_COMMAND\r\n"));
					log.Stderrf("invalid command from %s\n", req.conn.RemoteAddr());
			}
		}
		
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

