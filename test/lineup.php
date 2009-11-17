<?php
/*
 * @author Justin Poliey <jdp34@njit.edu>
 * @copyright 2009 Justin Poliey <jdp34@njit.edu>
 * @license http://www.opensource.org/licenses/mit-license.php The MIT License
 * @package Lineup
 */

/**
 * Wraps native Lineup errors in friendlier PHP exceptions
 */
class LineupException extends Exception {
}

/**
 * Lineup, a Lineup interface for the modest among us
 */
class Lineup {

	/**
	 * Socket connection to the Lineup server
	 * @var resource
	 * @access private
	 */
	private $__sock;
	
	/**
	 * Message commands, they are sent in a slightly different format to the server
	 * @var array
	 * @access private
	 */
	private $msg_cmds = array(
		'GIVE'
	);
	
	/**
	 * Creates a connection to the Lineup server on host {@link $host} and port {@link $port}.
	 * @param string $host The hostname of the server
	 * @param integer $port The port number of the server
	 */
	function __construct($host, $port = 9876) {
		$this->__sock = fsockopen($host, $port, $errno, $errstr);
		if (!$this->__sock) {
			throw new Exception("{$errno}: {$errstr}");
		}
	}
	
	function __destruct() {
		fclose($this->__sock);
	}
	
	function __call($name, $args) {
	
		/* Build the Lineup protocol command */
		$name = strtoupper($name);
		if (in_array($name, $this->msg_cmds)) {
			$priority = $args[0];
			$size = count($args) > 2 ? $args[2] : strlen($args[2]);
			$data = $args[1];
			$command = sprintf("%s %d %d\r\n%s\r\n", $name, $priority, $size, $data);
		}
		else {
			$command = sprintf("%s\r\n", $name);
		}
		
		/* Open a connection and execute the command */
		for ($written = 0; $written < strlen($command); $written += $fwrite) {
			$fwrite = fwrite($this->__sock, substr($command, $written));
			if (!$fwrite) {
				break;
			}
		}
		
		/* Parse the response based on the reply identifier */
		$reply = trim(fgets($this->__sock, 512));
		switch (substr($reply, 0, 1)) {
			/* Error reply */
			case '-':
				throw new LineupException(substr(trim($reply), 1));
				break;
			/* Inline reply */
			case '+':
				$response = substr(trim($reply), 1);
				break;
			/* Message reply */
			case '$':
				$read = 0;
				$size = substr($reply, 1);
				$response = "";
				do {
					$block_size = ($size - $read) > 1024 ? 1024 : ($size - $read);
					$response .= fread($this->__sock, $block_size);
					$read += $block_size;
				} while ($read < $size);
				fread($this->__sock, 2); /* discard crlf */
				break;
			/* Invalid reply */
			default:
				throw new LineupException("invalid server response: {$reply}");
				break;
		}
		/* Party on */
		return $response;
	}
	
}
		
