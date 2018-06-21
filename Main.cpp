/*                                                                                                              #THE KILLER BUNNY
Robert Boyer                                                                                                         (\_/)
CSS 432 - Program 5 (Winter 2016)                                                                                    (o o)
-------------------------------------------------------------------------------------------------------------       (     )
Creation Date: 3/1/2017
Date of Last Modification:
--------------------------------------------------------------------------------------------------------------------------------
Purpose: This is an individual project to implement a network application: an ftp client program based on the Internet Engineering Task Force RFC 959 protocol.
--------------------------------------------------------------------------------------------------------------------------------
Notes: The file transfer protocol (ftp) is defined in a 1985 IETF [plain text] document, RFC 959. It is highly recommended that you read this document, and
understand the FTP functions (described in Section 4.1), especially: USER, PASS, SYST, PASV, LIST, RETR, STOR, QUIT
--------------------------------------------------------------------------------------------------------------------------------
*/
#include <iostream>
#include <sys/types.h>    // socket, bind
#include <sys/socket.h>   // socket, bind, listen, inet_ntoa
#include <netinet/in.h>   // htonl, htons, inet_ntoa
#include <arpa/inet.h>    // inet_ntoa
#include <netdb.h>        // gethostbyname
#include <unistd.h>       // read, write, close
#include <string.h>       // bzero
#include <netinet/tcp.h>  // SO_REUSEADDR
#include <sys/uio.h>      // writev
#include <sys/poll.h> 	  // poll
#include <stdio.h>
#include <sys/time.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <string>
#include <fstream>
#include <iomanip>
#include "Timer.h"

using namespace std;
bool debug = true;						//Toggle calculated time in ftp commands GET & PUT statements on/off
bool connection = false;			//If connection is established
int sd;												//Client Socket descriptor
int fd; 											//Client File descriptor
char *hostName;			  				//Name of the host
int hostPort = 21;						//Default port value
struct hostent *host;

//-------------------(Passive mode)-----------------

int pasvSd1; // ls
int pasvSd2; // get
int pasvSd3; // put

/* 													add1 add2 add3 add4	pt1	pt2
  227 Entering Passive Mode (209.202.252.54.xxx.xxx)
*/
int add1, add2, add3, add4; //IP address
int pt1, pt2;								//Port number(s)

//--------------------(Functions)-------------------

int socketPoll(int sd_1);
int connectToLogin();

//--------------------------------------------------

int main(int argc, char* argv[]) {

	bool autoConnect = false;			//If automatic connection should be established: i.e. atleast hostname was provided

	//------------------------------------------------------- (Argument Validation) ------------------------------------------------------------------

	//Provided NO arguments
	/* then enters Main logic loop and must enter command 'open' or 'quit' */

	//Provided too many arguments
	if (argc > 3){
		cerr << "usage: ftp host-name [port]" << endl;
	}

	//Provided only 1 argument: hostname implies default port of 21
	if (argc == 2){
		hostName = argv[1];
		autoConnect = true;
	}

	//Provided 2 arguments: hostname and port
	if (argc == 3){

		//Validate port #
		if (atoi(argv[2]) <= 0){
			cerr << "ftp: connect: Connection refused" << endl;
		}
		else{
			hostName = argv[1];
			hostPort = atoi(argv[2]);
			autoConnect = true;
		}
	}

	// If a hostname and valid port has been provided call autoConnect()
	if (autoConnect){
		if (connectToLogin() == 0){ //Attempt connect and login
			connection = true; //if success
		}
	}

	//-------------------------------------------------------- (Main Logic Loop) ----------------------------------------------------------------------

	do {
		cout << "ftp> ";

		string command;
		cin >> command;

		char file[30]; // put & get
		char remote[30]; //put
		char folder[30]; //cd

		//-------------------------------------------------------- (help) ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

		if(command == "help"){
			cout << "Commands may be abbreviated. Commands are: " << endl;
			cout << "" << endl;
			cout << "cd" << endl;
			cout << "close" << endl;
			cout << "get" << endl;
			cout << "ls" << endl;
			cout << "open" << endl;
			cout << "put" << endl;
			cout << "quit" << endl;
			cout << "" << endl;
			continue;
		}

		//-------------------------------------------------------- (open) ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

		if(command == "open"){ //No hostname or port # was provided: No connection established, then ok to start one

			if(connection == false){ //no existing connection
				cout << "(to) ";

				//Get user's hostName ---> ftp.tripod.com
				char hostn[14];
				bzero(hostn, sizeof(hostn));
				cin >> hostn;
				hostName = hostn;

				//Attempt connect & login
				if (connectToLogin() == 0){
					connection = true; //if successfully connected & logged in
				}
			}
			else{ // connection already exists
				cout << "Already connected to " << hostName << ", use close first." << endl;
			}
			continue;
		}

		//--------------------------------------------------------- (cd) ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

		if(command == "cd" && connection == true){ //Must have connection established

			//Prompt & Read subdir
			//cout << "(remote-directory) ";
			cin >> folder;

			//Build (CWD subdir\r\n)
			char cwd[30];
			bzero(cwd, sizeof(cwd));
			strcpy(cwd, "CWD ");
			strcat(cwd, folder);
			strcat(cwd, "\r\n");

			//Send (CWD subdir\r\n)
			write(sd, (char*)&cwd, strlen(cwd));

			//Receive acknowledgement
			char buf_250[500];
			bzero(buf_250, sizeof(buf_250));
			read(sd, (char*)&buf_250, sizeof(buf_250));
			cout << buf_250; //Print response: 250 Directory set to '/subdir'.
			continue;
		}

		//--------------------------------------------------------- (ls) ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

		if(command == "ls" && connection == true){ //Must have connection established

			//Build (PASV\r\n)
			char pasv[30];
			bzero(pasv, sizeof(pasv));
			strcpy(pasv, "PASV");
			strcat(pasv, "\r\n");

			//Send (PASV\r\n)
			write(sd, (char*)&pasv, strlen(pasv));

			//Receive acknowledgement
			char buf_227[100];
			bzero(buf_227, sizeof(buf_227));
			read(sd, (char*)&buf_227, sizeof(buf_227));
			cout << buf_227;  //Print response: 227 Entering Passive Mode (x,x,x,x,x,x)

			//Save IP address & port number(s)
			sscanf(buf_227, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)", &add1, &add2, &add3, &add4, &pt1, &pt2);

			//Compute Port Number: [(5th element * 256) + 6th element]
			int serverPort = (pt1 * 256) + pt2;

			//Build Passive sending socket address
			struct sockaddr_in addr_ls;
			bzero((char*)&addr_ls, sizeof(addr_ls));
			addr_ls.sin_family = AF_INET;
			memcpy(&addr_ls.sin_addr, host->h_addr, host->h_length);
			addr_ls.sin_port = htons(serverPort);

			//Open a TCP socket
			pasvSd1 = socket(AF_INET, SOCK_STREAM, 0);
			if(pasvSd1 < 0){
					cout << "Problem creating socket" << endl;
					exit(0);
			}

			//Connect this TCP socket with the server passively
			int returnCode = connect(pasvSd1, (struct sockaddr*)&addr_ls, sizeof(addr_ls));
			if(returnCode < 0){
					cout << "Problem connecting to server" << endl;
					close(pasvSd1);
					exit(0);
			}

			int status;
			int pid = fork();

			if (pid < 0){
					cerr << "Error: Fork failed" << endl;
					exit(EXIT_FAILURE);
			}
			else if (pid == 0){ // child

					//Collect Directory List
					char temp[8192];
					bzero(temp, sizeof(temp));
					string f_dir_list = "";

					int length = 0;
					int total = 0;

					while (socketPoll(pasvSd1) > 0){

						bzero(temp, sizeof(temp));
						length = read(pasvSd1, temp, sizeof(temp));
						if (length == 0){
								break;
						}

						f_dir_list.append(temp);
						total += length;
					}

					//Print response: Directory List
					cout << f_dir_list << endl;

					//Close Socket
					close(pasvSd1);
					exit(EXIT_SUCCESS);
			}
			else{ // parent

					//Build (LIST\r\n)
					char list[10];
					bzero(list, sizeof(list));
					strcpy(list, "LIST");
					strcat(list, "\r\n");

					//Send (LIST\r\n)
					int sentList = write(sd, (char*)&list, strlen(list));

					//If successfully received
					if (sentList > 0){

							//Receive acknowledgement
							char buf_150[200];
							bzero(buf_150, sizeof(buf_150));
							read(sd, (char*)&buf_150, sizeof(buf_150));
							cout << buf_150; //Print response: 150 Opening ASCII mode data connection for LIST.

							wait(&status); //Wait for child to finish

							//Receive acknowledgement
							char buf_226[200];
							bzero(buf_226, sizeof(buf_226));
							read(sd, (char*)&buf_226, sizeof(buf_226));
							cout << buf_226; //Print response: 226 Transfer complete. (x bytes sent.)
					}
			}

			continue;
		}

		//--------------------------------------------------------- (get) ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

		if(command == "get" && connection == true){ //Must have connection established

			//Prompt & Read filename
			//cout << "(remote-file) ";
			cin >> file;

			string fileString = file;

			//Build (TYPE I\r\n)
			char type_i[12];
			bzero(type_i, sizeof(type_i));
			strcpy(type_i, "Type I");
			strcat(type_i, "\r\n");

			//Send (TYPE I\r\n)
			write(sd, (char*)&type_i, strlen(type_i));

			//Receive acknowledgement
			char buf_200[100];
			bzero(buf_200, sizeof(buf_200));
			read(sd, (char*)&buf_200, sizeof(buf_200));

			string rply = "200"; //Reply Code = The requested action has been successfully completed
			string resp = buf_200; //Set response to string for comparison

			//If response string doesn't contain 200 Reply Code, then stop
			if(!strstr(resp.c_str(), rply.c_str())) {
				cerr << "Error setting up binary mode" << endl;
				continue;
			}

			//Build (PASV\r\n)
			char pasv[30];
			bzero(pasv, sizeof(pasv));
			strcpy(pasv, "PASV");
			strcat(pasv, "\r\n");

			//Send (PASV\r\n)
			write(sd, (char*)&pasv, strlen(pasv));

			//Receive acknowledgement
			char buf_227[100];
			bzero(buf_227, sizeof(buf_227));
			read(sd, (char*)&buf_227, sizeof(buf_227));
			cout << buf_227;  //Print response: 227 Entering Passive Mode (x,x,x,x,x,x)

			//Save IP address & port number(s)
			sscanf(buf_227, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)", &add1, &add2, &add3, &add4, &pt1, &pt2);

			//Compute Port Number: [(5th element * 256) + 6th element]
			int serverPort = (pt1 * 256) + pt2;

			//Build Passive sending socket address
			struct sockaddr_in addr_g;
			bzero((char*)&addr_g, sizeof(addr_g));
			addr_g.sin_family = AF_INET;
			addr_g.sin_port = htons(serverPort);
			memcpy(&addr_g.sin_addr, host->h_addr, host->h_length);

			//Open a TCP socket
			pasvSd2 = socket(AF_INET, SOCK_STREAM, 0);
			if(pasvSd2 < 0) {
			 cout << "Error creating socket" << endl;
			 break;
		 }

			//Connect this TCP socket with the server passively
			int returnCode = connect(pasvSd2, (struct sockaddr*)&addr_g, sizeof(addr_g));
			if(returnCode < 0){
				cout << "Problem connecting to server" << endl;
				close(pasvSd2);
				break;
			}

			cout << buf_200; // 200 Type set to 'I' (IMAGE aka Binary)

			//Build (RETR filename\r\n)
			char retr[10];
			bzero(retr, sizeof(retr));
			strcpy(retr, "RETR ");
			strcat(retr, file);
			strcat(retr, "\r\n");

			//Send (RETR filename\r\n)
			write(sd, (char*)&retr, strlen(retr));

			//Receive acknowledgement
			char buf_150[200];
			bzero(buf_150, sizeof(buf_150));
			read(sd, (char*)&buf_150, sizeof(buf_150));
			cout << buf_150;  //Print response: 150 Opening BINARY mode data connection for 'filename'.

			rply = "150"; //Reply Code = File status okay; about to open data connection.
			resp = buf_150; //Set response to string for comparison

			//If response string doesn't contain 150 Reply Code, then stop
			if(!strstr(resp.c_str(), rply.c_str())) {
				cerr << "Error setting up file retrieval " << endl;
				close(pasvSd2);
				continue;
			}


			mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
			fd = open(fileString .c_str(), O_WRONLY | O_CREAT, mode);

			if (fd < 0){

				cerr << "local: " << fileString << ": Permission denied" << endl;
				close(pasvSd2);
				close(fd);
				fd = -1;
				continue;
			}

			Timer timer;
			double time = 0;

			int status;
			int pid = fork();

			if (pid < 0){
					cerr << "Error: Fork failed" << endl;
					exit(EXIT_FAILURE);
			}
			else if (pid == 0) { // child

				timer.Start();

				char buffer[8192];
				int received = 0;
				int total = 0;

				while (socketPoll(pasvSd2) > 0){

				//Receive data in buffered chunks
				bzero(buffer, sizeof(buffer));
				received = read(pasvSd2, buffer, sizeof(buffer));
				if (received == 0){
					break;
				}
					//Write to disk before receiving the next chunk
					lseek(fd, 0, SEEK_END);
					write(fd, buffer, received);

					total += received;
				}

				time = timer.End();

				close(pasvSd2); //Close passive data socket
				close(fd); //Close file descriptor
				fd = -1;

				//Receive acknowledgement
				char buf_226[200];
				bzero(buf_226, sizeof(buf_226));
				read(sd, (char*)&buf_226, sizeof(buf_226));
				cout << buf_226;  //Print response: 226 Transfer complete. (x bytes sent.)

				//Caclulate Time
				if(debug){
					double secs = time / 1000000;
					cout << fixed << total << " bytes received in " <<
					setprecision(2) << secs << " secs (" <<
					setprecision(4) << total / secs / 1024 << " kB/s)" << endl;
				}

				exit(EXIT_SUCCESS);
			}
			else { // parent
					wait (&status);
			}

			close(pasvSd2); //Close passive data socket
			continue;
		}

		//--------------------------------------------------------- (put) ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

		if(command == "put" && connection == true){ //Must have connection established

			//Prompt & Read filename
			cout << "(local-file) ";
			cin >> file;
			cout << "(remote-file) ";
			cin >> remote;

			string fileString = file;

			fd = open(fileString.c_str(), O_RDONLY);
			if(fd < 0){
				cerr << "local: " << fileString << ": No such file or directory" << endl;
				continue;
			}

			//Build (TYPE I\r\n)
			char type_i[12];
			bzero(type_i, sizeof(type_i));
			strcpy(type_i, "Type I");
			strcat(type_i, "\r\n");

			//Send (TYPE I\r\n)
			write(sd, (char*)&type_i, strlen(type_i));

			//Receive acknowledgement
			char buf_200[100];
			bzero(buf_200, sizeof(buf_200));
			read(sd, (char*)&buf_200, sizeof(buf_200));

			string rply = "200"; //Reply Code = The requested action has been successfully completed
			string resp = buf_200; //Set response to string for comparison

			//If response string doesn't contain 200 Reply Code, then stop
			if(!strstr(resp.c_str(), rply.c_str())) {
				cerr << "Error setting up binary mode" << endl;
				continue;
			}

			//Build (PASV\r\n)
			char pasv[30];
			bzero(pasv, sizeof(pasv));
			strcpy(pasv, "PASV");
			strcat(pasv, "\r\n");

			//Send (PASV\r\n)
			write(sd, (char*)&pasv, strlen(pasv));

			//Receive acknowledgement
			char buf_227[100];
			bzero(buf_227, sizeof(buf_227));
			read(sd, (char*)&buf_227, sizeof(buf_227));
			cout << buf_227;  //Print response: 227 Entering Passive Mode (x,x,x,x,x,x)

			//Save IP address & port number(s)
			sscanf(buf_227, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)", &add1, &add2, &add3, &add4, &pt1, &pt2);

			//Compute Port Number: [(5th element * 256) + 6th element]
			int serverPort = (pt1 * 256) + pt2;

			//Build Passive sending socket address
			struct sockaddr_in addr_p;
			bzero((char*)&addr_p, sizeof(addr_p));
			addr_p.sin_family = AF_INET;
			addr_p.sin_port = htons(serverPort);
			memcpy(&addr_p.sin_addr, host->h_addr, host->h_length);

			//Open a TCP socket
			pasvSd3 = socket(AF_INET, SOCK_STREAM, 0);
			if(pasvSd3 < 0) {
			 cout << "Error creating socket" << endl;
			 exit(0);
			}

			//Connect this TCP socket with the server passively
			int returnCode = connect(pasvSd3, (struct sockaddr*)&addr_p, sizeof(addr_p));
			if(returnCode < 0){
				cout << "Problem connecting to server" << endl;
				close(pasvSd3);
				break;
			}

			cout << buf_200; // 200 Type set to 'I' (IMAGE aka Binary)

			//Build (STOR filename\r\n)
			char stor[100];
			bzero(stor, sizeof(stor));
			strcpy(stor, "STOR ");
			strcat(stor, file);
			strcat(stor, "\r\n");

			//Send (STOR filename\r\n)
			write(sd, (char*)&stor, strlen(stor));

			//Receive acknowledgement
			char buf_150[500];
			bzero(buf_150, sizeof(buf_150));
			read(sd, (char*)&buf_150, sizeof(buf_150));
			cout << buf_150;  //Print response: 150 Opening BINARY mode data connection for 'filename'.

			rply = "150"; //Reply Code = File status okay; about to open data connection.
			resp = buf_150; //Set response to string for comparison

			//If response string doesn't contain 150 Reply Code, then STOP
			if(!strstr(resp.c_str(), rply.c_str())) {
				cerr << "Error setting up file storage " << endl;
				close(pasvSd3);
				continue;
			}

			Timer timer;
			double time = 0;

			int status;
			int pid = fork();

			if (pid < 0){
					cerr << "Error: Fork failed" << endl;
					exit(EXIT_FAILURE);
			}
			else if (pid == 0){ // child

				timer.Start();

				//Get length of file
				int length = lseek(fd, 0, SEEK_END);
				lseek(fd, 0, 0); //reset to beginning

				char *buffer = new char[length];

				//Read data as a block
				read(fd, buffer, length);

				//Transmit the buffer
				int fileSize = write(pasvSd3, buffer, length);

				delete[] buffer;
				buffer = NULL;

				time = timer.End();

				shutdown(pasvSd3, SHUT_WR);
				close(pasvSd3);

				close(fd);
				fd = -1;

				//Receive acknowledgement
				char buf_226[500];
				bzero(buf_226, sizeof(buf_226));
				read(sd, (char*)&buf_226, sizeof(buf_226));
				cout << buf_226; //Print response: 226 Transfer complete. (x bytes sent.)

				//Caclulate Time
				if(debug){
					double secs = time / 1000000;
					cout << fixed << fileSize << " bytes sent in " <<
					setprecision(2) << secs << " secs (" <<
					setprecision(4) << fileSize / secs / 1024 << " kB/s)" << endl;
				}

				exit(EXIT_SUCCESS);
			}
			else { // parent
					wait (&status);
			}

			close(pasvSd3); //Close passive data socket
			continue;
		}

		//-------------------------------------------------------- (close) ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

		if(command == "close" && connection == true){ //Must have connection established

			//Build (QUIT\r\n)
			char quit[10];
			bzero(quit, sizeof(quit));
			strcpy(quit, "QUIT");
			strcat(quit, "\r\n");

			//Send (QUIT\r\n)
			write(sd, (char*)&quit, strlen(quit));

			//Receive acknowledgement
			char buf_221[20];
			bzero(buf_221, sizeof(buf_221));
			read(sd, (char*)&buf_221, sizeof(buf_221));
			cout << buf_221; //Print response: 221 Goodbye...

			shutdown(sd, SHUT_WR);
			connection = false;
			continue; //continue ftp
		}

		//-------------------------------------------------------- (quit) ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

		if(command == "quit"){

			//Build (QUIT\r\n)
			char quit[10];
			bzero(quit, sizeof(quit));
			strcpy(quit, "QUIT");
			strcat(quit, "\r\n");

			//Send (QUIT\r\n)
			write(sd, (char*)&quit, strlen(quit));

			//Receive acknowledgement
			char buf_221[20];
			bzero(buf_221, sizeof(buf_221));
			read(sd, (char*)&buf_221, sizeof(buf_221));
			cout << buf_221; //Print response: 221 Goodbye...
			break; //exit ftp
		}

		//----------------------------------------------------- (non-valid) ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

		else{ //Non-valid commands

			if(connection == false){ //Not connected
				cout << "Not connected." << endl;
				continue;
			}
			else{ //Unrecognizable command
				cout << "?Invalid command" << endl;
				continue;
			}
		}

	} while (true);

	close(sd);
	return 0;
}

//-------------------------------------------------------- (Connect to Login) --------------------------------------------------------------------

// Function (1) establishes a connection to specified server (2) completes user login & password
int connectToLogin(){

	cout << "Connected to " << hostName << endl;

	// Get Hostent structure to communicate to server.
	host = gethostbyname(hostName);
	if (host == NULL){
		cerr << "ftp:  "<< hostName << " Name or service not known"<< endl;
		return -1;
	}

	// Build the sending socket address of the client.
	struct sockaddr_in addr_cl;
	bzero((char *)&addr_cl, sizeof(addr_cl));
	addr_cl.sin_family = AF_INET;
	addr_cl.sin_addr.s_addr = inet_addr(inet_ntoa(*(struct in_addr*) (*host->h_addr_list)));
	addr_cl.sin_port = htons(hostPort);

	// Open a TCP socket.
	sd = socket(AF_INET, SOCK_STREAM, 0);
	if (sd < 0){
		cerr << "Problem creating socket" << endl;
		close(sd);
		return -1;
	}

	// Connect this TCP socket with the server
	int returnCode = connect(sd, (sockaddr *)&addr_cl, sizeof(addr_cl));
	if (returnCode < 0){ //negative return indicates failure
		cerr << "Problem connecting to server" << endl;
		close(sd);
		return -1;
	}

	//Read from server & print response: 220 Welcome to Tripod us FTP.
	char buf_220[1500];
	bzero(buf_220, sizeof(buf_220));
	read(sd, (char*)&buf_220, sizeof(buf_220));
	cout << buf_220;

	//Prompt & Read user_name
	string userString(getlogin()); //To display system username
	cout << "Name ("<< hostName << ":" << userString << "): ";
	char name[9]; // (css432w17)
	bzero(name, sizeof(name));
	cin >> name;

	//Build (USER user_name\r\n)
	char user[16]; // (USER css432w17\r\n)
	bzero(user, sizeof(user));
	strcpy(user, "USER ");
	strcat(user, name);
	strcat(user, "\r\n");

	//Send (USER user_name\r\n)
	write(sd, (char*)&user, strlen(user));

	//Read from server & print response: 331 Username set to (user_name). Now enter your password.
	char buf_331[1500];
	bzero(buf_331, sizeof(buf_331));
	read(sd, (char*)&buf_331, sizeof(buf_331));
	cout << buf_331;

	//Prompt & Read user_password
	cout << "Password: ";
	char pword[9]; // UWB0th3ll = 9
	bzero(pword, sizeof(pword));
	cin >> pword;

	//Build (PASS user_password\r\n)
	char pass[16];
	bzero(pass, sizeof(pass));
	strcpy(pass, "PASS ");
	strcat(pass, pword);
	strcat(pass, "\r\n");

	//Send (PASS user_password\r\n)
	write(sd, (char*)&pass, strlen(pass));

	//Read from server
	char buf_230[1500];
	bzero(buf_230, sizeof(buf_230));
	read(sd, (char*)&buf_230, sizeof(buf_230));

	string rply = "501"; //Reply Code = Syntax error in parameters or arguments
	string resp = buf_230; //Set response to string for comparison

	//If response string doesn't contain 501 Reply Code, password accepted
	if(!strstr(resp.c_str(), rply.c_str())){

		cout << buf_230;

		//Reads for more data: IMPORTANT NOTICE section
		while(socketPoll(sd) > 0){
			bzero(buf_230, sizeof(buf_230));
			read(sd, (char*)&buf_230, sizeof(buf_230));
			cout << buf_230 << endl;
		}
		//Unable to Read data
		if(socketPoll(sd) == -1){
			cerr << "Error polling the socket" << endl;
		}

		//Build (SYST \r\n)
		char syst[30];
		bzero(syst, sizeof(syst));
		strcpy(syst, "SYST ");
		strcat(syst, "\r\n");

		//Send (SYST \r\n)
		write(sd, (char*)&syst, strlen(syst));

		//Read & Print response: 215 UNIX Type: L8
		char buf_215[500];
		bzero(buf_215, sizeof(buf_215));
		read(sd, (char*)&buf_215, sizeof(buf_215));
		cout << buf_215;

		return 0;
	}
	else{ //501
		//Expected output following: $ftp ftp.tripod.com example
		cout << "Login failed." << endl;
		return -1;
	}

}

//----------------------------------------------------------- (Socket Poll) -----------------------------------------------------------------------

// Function checks socket descriptor for data and return a positive number if readable, otherwise returns 0 or a negative number
int socketPoll(int sd_1) {

    struct pollfd ufds;
    ufds.fd = sd_1;             	// a socket descriptor to exmaine for read
    ufds.events = POLLIN; 				// check if this sd is ready to read
    ufds.revents = 0;						// simply zero-initialized

    return poll(&ufds, 1, 1000); 		//poll this socket for 1000msec
}
