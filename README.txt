/********************START HEADER********************/
/* \file README
	
	\ author Zulfami, 	b.zulfamiashrafi 
	\ author Brandon Poon, 	b.poon
	\ author Gabriel Peh, 	peh.j
	
	\par email: b.zulfamiashrafi@digipen.edu
	\par email: b.poon@digipen.edu
	\par email: peh.j@digipen.edu
	
	\date 19 March 2025

	\brief Copyright (C) 2025 Digipen Institute of Technology

/********************END HEADER********************/
Zulfami: 
	- Set up and configured the TCP connection between the server and client.
	- Managed ACK (Acknowledgment) handling to ensure reliable data transfer.
	- Developed and optimized connection initialization and teardown processes.
	- Conducted testing, debugging, and performance analysis to improve network stability.
Brandon:
	- Developed server-side multi-threading to handle concurrent client requests effectively.
	- Implemented packet queuing mechanisms to manage incoming data efficiently.
	- Conducted stress testing, debugging, and performance optimization for the UDP transmission process.
	- Enhanced error detection and recovery to minimize packet loss and improve reliability.
Gabriel:
	- Developed the client-side message processing and logging system.
	- Implemented message validation and error handling to ensure accurate communication.
	- Conducted extensive testing and debugging to verify client-server interactions.

Instructions
1. Unzip Assignment3 into folder. 
2. Create a folder under C: drive
3. In CMD, go to C:\Assignment3
4. Connect VIA cable and set static IP Address to 192.163.0.3, to prevent conflict during testing.
5. Run Server through solution file by selecting start up project and run as startup project
6. (Server) 
6.1 Server TCP Port Number: 9000
6.2 Server UDP Port Number: 9001
6.3 Download path: ./
6.4 Packet loss rate: 0.1
6.5 Ack timer: 10
7. (Client)
7.1 Server IP Address: 192.168.0.4
7.2 Server TCP Port Number: 9000
7.3 Server UDP Port Number: 9001
7.4 Client UDP port number: 9010
7.5 Path to store files: .\
7.6 Packet loss rate: 0.2
7.7 /l
7.8 /d 192.168.0.3:9010 Hello.txt
7.9 /q

/l (list files)
/d 192.168.0.3:9010 Hello.txt
(download Hello.txt file via udp port with ip address 192.168.0.3 and port number 9010)
/q (quit)


