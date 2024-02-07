End-To-End Detection of Network Compression
=============================================

## How to Build Program

1. Build the `mjson.o` file:
    ```
    gcc -g -c -o mjson.o mjson.c
    ```

2. Make the executable for client, server, and standalone:
    ```
    make client
    make server
    make standalone
    ```

3. Run the server:
    ```
    ./server myconfig.json
    ```

4. Run the client:
    ```
    ./client myconfig.json
    ```

5. After the server and client finished executing, run the standalone program.

6. Run standalone using `sudo`:
    ```
    sudo ./standalone myconfig.json
    ```

## Incomplete Required Features of Program

1. Not able to receive RST packets from closed ports:
   - Unable to implement timer correctly.

2. Minor:
   - Was not able to exit the while loop on the server side when receiving a packet train without receiving a "done" message from the client.
   - Aim to find a way to implement exiting the while loop while the last packet of the packet train is receiving without having to waste resources sending a "done" message.

## What to Expect While Running Programs (Print Statements Indicating Success or Failure)

- `[+]` indicates successful operation. If a print statement does not have a `[+]`, an error/failure occurred during the operation and exits the program.

### Client:

1. Pre-Probing Phase:
    - Socket creation.
    - Connection to server.
    - Sending JSON file contents to server.
    - Closing connection to server.

2. Probing Phase:
    - Socket creation.
    - Setting DF flag for UDP packets.
    - Sending the first packet of Low Entropy Train.
    - Sending the last packet of Low Entropy Train.
    - Sending "done" message to server.
    - Waiting Inter-Measurement time.
    - Sending the first packet of High Entropy Train.
    - Sending the last packet of High Entropy Train.
    - Sending "done" message to server.
    - Statement indicating if both packet trains were sent successfully.

3. Post-Probing Phase:
    - Socket creation.
    - Connection to server.
    - Prints the findings of compression detection.
    - Closing connection to server.

### Server:

1. Pre-Probing Phase:
    - Prints Server IP and TCP for reference and confirmation of correct parsing of JSON file.
    - Socket creation.
    - Binding socket to port.
    - Listening for any incoming connections from client.
    - Prints IP and Port # of client if connection is established.
    - Prints the file contents received from client.
    - Statement if parsing of the JSON was successful.
    - Closing connection to client.

2. Probing Phase:
    - Socket creation.
    - Binding socket to port.
    - Number of packets received from Low Entropy Train.
    - Number of packets received from High Entropy Train.
    - Compression time of Low Entropy Train.
    - Compression time of High Entropy Train.

3. Post-Probing Phase:
    - Compression time of Low Entropy Train.
    - Compression time of High Entropy Train.
    - Socket creation.
    - Binding socket to port.
    - Listening to client.
    - Prints IP and Port # of client if connection is established.
    - Sending compression findings to client.
    - Closing connection to client.
