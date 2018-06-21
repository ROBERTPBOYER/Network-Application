/*                                                                                                              #THE KILLER BUNNY
Robert Boyer                                                                                                         (\_/)
CSS 432 - Program 5 (Winter 2016)                                                                                    (o o)
-------------------------------------------------------------------------------------------------------------       (     )
Creation Date: 3/10/2017
Date of Last Modification:
--------------------------------------------------------------------------------------------------------------------------------
Purpose: This is an individual project to implement a network application: an ftp client program based on the Internet Engineering Task Force RFC 959 protocol.
--------------------------------------------------------------------------------------------------------------------------------
Notes: The file transfer protocol (ftp) is defined in a 1985 IETF [plain text] document, RFC 959. It is highly recommended that you read this document, and
understand the FTP functions (described in Section 4.1), especially: USER, PASS, SYST, PASV, LIST, RETR, STOR, QUIT
--------------------------------------------------------------------------------------------------------------------------------
*/
#include "Timer.h"

Timer::Timer( ) {

    startTime.tv_sec = 0;
    startTime.tv_usec = 0;
    endTime.tv_sec = 0;
    endTime.tv_usec = 0;
}
//Function begins timer
void Timer::Start() {

    gettimeofday(&startTime, NULL);
}

//Function calculates the interval which is the difference between the start and the current end time
long Timer::End() {

    gettimeofday(&endTime, NULL);
    long interval =
        (endTime.tv_sec - startTime.tv_sec) * 1000000 +
        (endTime.tv_usec - startTime.tv_usec);
    return interval;
}
