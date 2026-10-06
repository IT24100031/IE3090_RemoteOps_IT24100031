# RemoteOps

## IE3090 Network Programming Assignment

### Student Information

* Registration Number: IT24100031
* Module: IE3090 Network Programming

## Personalization

 Item                  Value                    
 
 Registration Number : IT24100031               
 Agent TCP Port      : 9410                     
 Session ID (SID)    : 1300                     
 Authentication Token: OPS-0031                 
 Agent Source        : agent_031.c              
 Controller Source   : controller_031.c         
 Makefile            : Makefile_031             
 Log File            : remoteops_IT24100031.log 
 Agent Storage       : ./agentfiles/IT24100031/ 

## Project Description

RemoteOps is a remote system monitoring and management tool developed using C and the BSD sockets API.

The system consists of:

* **Agent** - runs on the managed Linux machine and provides remote monitoring and management services.
* **Controller** - connects to the Agent and sends commands.
* **TCP/IP** - used as the main communication protocol.
* **UDP** - used for periodic monitoring information.

The Agent supports multiple simultaneous Controller connections using POSIX threads.

## Main Features

### Authentication

The Controller must authenticate before using protected commands.

Authentication token:

```text
OPS-0031
```

Successful authentication produces:

```text
OK AUTHENTICATED SID:1300
```

### SYSINFO

Returns system information including:

* CPU load
* Memory usage
* System uptime

Example:

```text
OK SYSINFO 0.54 1646 83460 SID:1300
```

### LISTPROC

Lists currently running processes on the Agent.

Example command:

```text
LISTPROC
```

### EXEC

The following commands are allowed:

```text
DATE
UPTIME
DISKFREE
HOSTNAME
WHOAMI
```

Only these commands are permitted through the EXEC interface.

For example:

```text
EXEC DATE
EXEC UPTIME
EXEC DISKFREE
EXEC HOSTNAME
EXEC WHOAMI
```

Commands outside the whitelist are rejected.

Example:

```text
EXEC LS
```

Response:

```text
ERR 002 COMMAND_NOT_ALLOWED SID:1300
```

### PUT

PUT uploads a file from the Controller to the Agent.

Example:

```text
PUT test_upload.txt 44
```

Uploaded files are stored under:

```text
./agentfiles/IT24100031/
```

### GET

GET downloads a file from the Agent to the Controller.

Example:

```text
GET test_upload.txt
```

### MONITOR

UDP monitoring can be started using:

```text
MONITOR START 9001
```

The Agent responds:

```text
OK MONITOR_STARTED SID:1300
```

The Agent then sends monitoring information through UDP every 5 seconds.

Example UDP packet:

```text
SYSINFO 0.54 1646 83460 SID:1300
```

Monitoring can be stopped using:

```text
MONITOR STOP
```

Response:

```text
OK MONITOR_STOPPED SID:1300
```

### QUIT

The Controller can gracefully disconnect using:

```text
QUIT
```

The Agent responds:

```text
OK BYE SID:1300
```

## Concurrency

The Agent uses POSIX threads to support multiple simultaneous Controller connections.

Each accepted TCP connection is handled by a separate thread.

The system was tested using multiple simultaneous Controller connections.

## Communication

### TCP

TCP is the primary communication protocol between the Controller and Agent.

TCP is used for:

* HELLO
* AUTH
* SYSINFO
* LISTPROC
* EXEC
* PUT
* GET
* MONITOR START
* MONITOR STOP
* QUIT

TCP commands and responses use a line-based protocol.

### UDP

UDP is used for periodic monitoring information after the Controller starts monitoring.

The Controller uses UDP port:

```text
9001
```

Monitoring packets are sent every 5 seconds.

Each monitoring packet contains:

```text
SYSINFO <CPU_LOAD> <MEMORY_USED_MB> <UPTIME_SECONDS> SID:1300
```

## File Transfer

The RemoteOps system supports both uploading and downloading files.

Uploaded files are stored in:

```text
./agentfiles/IT24100031/
```

The test file used during development was:

```text
test_upload.txt
```

The file size was:

```text
44 bytes
```

File integrity was verified after downloading by comparing the original and downloaded files.

SHA-256 verification was also performed.

## Logging

Timestamped events are recorded in:

```text
remoteops_IT24100031.log
```

The log records important Agent events such as:

* Controller connections
* Commands received
* Authentication
* Monitoring start
* Monitoring stop
* Controller disconnection

## Compilation

The project can be compiled using the provided Makefile.

### Build

```bash
make -f Makefile_031
```

### Clean

```bash
make -f Makefile_031 clean
```

### Rebuild

```bash
make -f Makefile_031 rebuild
```

### Manual compilation

Compile the Agent:

```bash
gcc -Wall -Wextra -pthread -o agent_031 agent_031.c
```

Compile the Controller:

```bash
gcc -Wall -Wextra -o controller_031 controller_031.c
```

## Running the System

### 1. Start the Agent

On the Linux machine:

```bash
./agent_031
```

The Agent listens on TCP port:

```text
9410
```

The Agent displays:

```text
RemoteOps Agent started.
TCP listening port: 9410
SID: 1300
Authentication token: OPS-0031
```

### 2. Start the Controller

Open another terminal:

```bash
./controller_031
```

The Controller connects to:

```text
127.0.0.1:9410
```

### 3. HELLO

The Controller sends:

```text
HELLO
```

The Agent responds with the HELLO response.

### 4. Authentication

The Controller sends:

```text
AUTH OPS-0031
```

A successful authentication response contains:

```text
OK AUTHENTICATED SID:1300
```

### 5. System Information

Run:

```text
SYSINFO
```

### 6. Process List

Run:

```text
LISTPROC
```

### 7. Execute Whitelisted Commands

Run:

```text
EXEC DATE
```

```text
EXEC UPTIME
```

```text
EXEC DISKFREE
```

```text
EXEC HOSTNAME
```

```text
EXEC WHOAMI
```

### 8. Test Command Rejection

Run:

```text
EXEC LS
```

The command should be rejected because LS is not part of the permitted EXEC whitelist.

### 9. Upload a File

Example:

```text
PUT test_upload.txt 44
```

### 10. Download a File

Example:

```text
GET test_upload.txt
```

### 11. Start UDP Monitoring

Run:

```text
MONITOR START 9001
```

The Controller receives UDP monitoring packets every 5 seconds.

### 12. Stop UDP Monitoring

Run:

```text
MONITOR STOP
```

### 13. Disconnect

Run:

```text
QUIT
```

## Testing

The following functionality was tested:

* TCP connection
* HELLO exchange
* Authentication
* Invalid authentication
* SYSINFO
* LISTPROC
* EXEC DATE
* EXEC UPTIME
* EXEC DISKFREE
* EXEC HOSTNAME
* EXEC WHOAMI
* EXEC command rejection
* PUT
* GET
* File integrity verification
* Multiple simultaneous Controller connections
* UDP monitoring
* MONITOR START
* MONITOR STOP
* QUIT
* Timestamp logging

### File Integrity Test

The uploaded and downloaded test files were compared using:

```bash
cmp test_upload.txt downloaded_test.txt
```

No difference was reported.

SHA-256 verification was also performed.

## Project Structure

```text
IE3090_RemoteOps/
│
├── agent_031.c
├── controller_031.c
├── Makefile_031
├── README.md
├── .gitignore
├── test_upload.txt
├── remoteops_IT24100031.log
│
└── agentfiles/
    └── IT24100031/
        └── test_upload.txt
```

## Build Requirements

The project requires:

* Linux operating system
* GCC compiler
* BSD/POSIX socket API
* POSIX thread support
* pthread library

The Agent is compiled with:

```text
-pthread
```

## Student

**Registration Number:** IT24100031

**Module:** IE3090 Network Programming
The project was tested for TCP communication, authentication,
SYSINFO, LISTPROC, EXEC, PUT, GET, concurrent controllers,
UDP monitoring, graceful disconnection, and file integrity.

