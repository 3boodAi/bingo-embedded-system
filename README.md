# IoT Interactive Bingo System

## Overview
This is an embedded systems academic project for Bahçeşehir University. It implements an Interactive Bingo System connecting a physical hardware board with a web-based client interface.

## System Architecture
The project utilizes a 3-node structure:
1. **Physical Board (Arduino UNO + ESP8266):** Handles the physical game interactions, generating random numbers, and displaying physical victory effects.
2. **Relay Server (Node.js WebSocket):** A central server acting as a bridge. It manages connections, broadcasts generated numbers from the hardware to web clients, and relays victory claims back to the hardware.
3. **Web Interface:** The player's client application where they can play the Bingo game and claim victory.

## Development Phases
- [x] Phase 1: Central Server (Node.js Relay Server)
- [ ] Phase 2: Web Client (Web Interface)
- [ ] Phase 3: Embedded C++ (Arduino + ESP8266)

## Setup and Run
### Central Server
1. Ensure Node.js is installed.
2. Navigate to the project root directory.
3. Install dependencies: `npm install`
4. Start the server: `node server.js` or `npm start`
