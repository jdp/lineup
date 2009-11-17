package server

import(
	"os";
	"fmt";
	"log";
	"net";
	"regexp";
	"strings";
	"bytes";
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
 * Utility function to expect a byte sequence from the client.
 */
func (s *Server) expect(req *Request, src *bufio.Reader, expected []byte) (r bool, err os.Error) {
	b := make([]byte, len(expected));
	_, err = src.Read(b);
	if err != nil {
		return;
	}
	if bytes.Compare(b, expected) == 0 {
		r = true;
	}
	else {
		err = os.NewError(fmt.Sprintf("expected %v, got %v", expected, b));
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
	if _, err := s.expect(req, src, []byte{'\n'}); err != nil {
		return;
	}
	message := make([]byte, size);
	_, err = src.Read(message);
	if err != nil {
		return;
	}
	if len(message) < size {
		err = os.NewError(fmt.Sprintf("message not fully read: %d of %d bytes read", len(message), size));
		return;
	}
	if _, err = s.expect(req, src, []byte{'\r','\n'}); err != nil {
		return;
	}
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
	if _, err := s.expect(req, src, []byte{'\n'}); err != nil {
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
 * Handles PING commands from clients.
 *
 * Successful response:
 *   +PONG\r\n
 *
 * Unsuccessful response:
 *  -<error-message>\r\n
 */
func (s *Server) handlePingCmd(req *Request, src *bufio.Reader) (err os.Error) {
	if _, err := s.expect(req, src, []byte{'\n'}); err != nil {
		return;
	}
	req.conn.Write(strings.Bytes("+PONG\r\n"));
	return;
}

/*
 * Handles EXIT commands from clients.
 * No response.
 */
func (s *Server) handleExitCmd(req *Request, src *bufio.Reader) (err os.Error) {
	_, err = s.expect(req, src, []byte{'\n'});
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
	
	req.conn.SetReadTimeout(int64(s.timeout) * 1e8);
	
	for {
		buf := bufio.NewReader(req.conn);
		line, err := buf.ReadBytes('\r');
		if err != nil {
			switch {
				case err == os.EOF:
					disconnected = true;
				case err == os.EAGAIN:
					disconnected = true;
				default:
					log.Stderrf("read error: %v\n", err);
					disconnected = true;
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
					if err := s.handlePingCmd(req, buf); err != nil {
						log.Stderrf("PING: failed from %s: %s\n", req.conn.RemoteAddr(), err);
					}
					else {
						log.Stdoutf("PING: successful from %s\n", req.conn.RemoteAddr());
					}

				// Handle the EXIT command
				case exitCmd.Match(line):
					if err := s.handleExitCmd(req, buf); err != nil {
						log.Stderrf("EXIT: failed from %s: %s\n", req.conn.RemoteAddr(), err);
					}
					else {
						log.Stdoutf("EXIT: successful from %s\n", req.conn.RemoteAddr());
					}
					disconnected = true;
			
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
			return;
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

