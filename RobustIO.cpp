#include "RobustIO.h"


/*
 * C buffered read, this is a standard approach used in
 * quite a few textbooks.
 * 
 * Attempts to read n bytes into a buffer from a file descriptor. 
 * This code is best for networking functionality where n bytes need 
 * to be handled or the read could return corrupt data.
 * 
 */
int RobustIO::readn(int fd, char* buffer, int count) {
    char* ptr = buffer;
	int left = count;
    int n;

	while(left > 0) {
		n = read(fd, ptr, left);
		if(n == 0)
			break;
		if(n < 0) {
			if(errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
				n = 0;
			} else {
				return(-1);
			}
		}
		left -= n;
		ptr += n;
	}
	return (count-left);
}

/*
 * C buffered write, this is a standard approach used in
 * quite a few textbooks.
 * 
 * Attempts to write n bytes from a buffer to a file descriptor. 
 * This code is best for networking functionality where n bytes need 
 * to be handled or the read could return corrupt data.
 * 
 */
int RobustIO::writen(int fd, char* buffer, int count) {
    char* ptr = buffer;
	int left = count;
    int n;

	while(left > 0) {
		n = write(fd, ptr, left);
		if(n < 0) {
			if(errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
				n = 0;
			} else {
				return(-1);
			}
		}
		left -= n;
		ptr += n;
	}
	return (count-left);
}

int RobustIO::write_string(int fd, std::string s) {
    int len = s.length() + 1;
    int buf = htonl(len);

    // Write string length to the buffer
    writen(fd, (char*) &buf, sizeof(len));

    // Write the entire std::string to the buffer.
    return writen(fd, (char*) s.c_str(), len);
}

std::string RobustIO::read_string(int fd) {
    int len;

    // Read string length, convert to host order
    readn(fd, (char*) &len, 4);
    len = ntohl(len);

    // Read len bytes, storing the string into a buffer
    char* buffer = new char[len];
    readn(fd, buffer, len);
    
    std::string s = buffer;
    return s;
}