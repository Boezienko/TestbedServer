# TestingServer
This repository has my testing server project, where I have made a server script to run on one machine, and a client script to run on another machine. The client is able to connect to the server and then look at the test cases on the server; additionally, the client can submit their code to be tested against a testcase file. Once the tests have been run, the results are sent back to the client.

## testbedServer.c
This file is the actual server script that listens for connections, then creates a thread and socket for each connection for communication with a client using a tcp stream socket.

## client.c
This file is for clients to run to connect with the server to test their programs.<br> It should be compiled like this:gcc client.c -o <outputFileName>. Then run the program like this: ./<outputFileName> <fileToTest> <testcaseName>. Ensure that the fileToTest is in the same directory as client.c
