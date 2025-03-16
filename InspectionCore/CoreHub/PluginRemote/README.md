# Plugin Remote Viewer

An Electron application that receives messages from the PluginTemp C++ application via shared memory and displays them in a web interface.

## Features

- Real-time message display from PluginTemp
- Connection status indicator
- Message count tracking
- Clear messages functionality

## Prerequisites

- Node.js (v14 or later)
- npm (v6 or later)
- Boost libraries (for shared memory communication)
- C++ compiler (GCC, Clang, or MSVC)

### Installing Boost on macOS

```bash
brew install boost
```

### Installing Boost on Linux

```bash
sudo apt-get install libboost-all-dev
```

### Installing Boost on Windows

Download the Boost installer from the [official website](https://www.boost.org/users/download/) and follow the installation instructions.

## Installation

1. Clone the repository
2. Navigate to the PluginRemote directory
3. Install dependencies:

```bash
npm install
```

This will also build the native addon for shared memory communication.

## Usage

1. Start the PluginTemp application in sender mode:

```bash
cd ../CoreHub
./PluginTemp --send
```

2. In a separate terminal, start the PluginRemote Electron app:

```bash
cd PluginRemote
npm start
```

3. The Electron app will connect to the shared memory channel created by PluginTemp and display any messages sent.

## Development

- Run in development mode (with DevTools):

```bash
npm run dev
```

- Rebuild the native addon:

```bash
npm run build
```

- Clean build files:

```bash
npm run clean
```

## How It Works

1. The PluginTemp C++ application creates a shared memory segment and sends messages to it.
2. The PluginRemote Electron app uses a native Node.js addon to connect to the same shared memory segment.
3. When a message is received in shared memory, it's passed to the Electron renderer process via IPC.
4. The renderer process displays the message in the web interface.

## Troubleshooting

- If the app fails to connect to shared memory, ensure that PluginTemp is running in sender mode.
- On macOS and Linux, you may need to manually clean up shared memory segments if the app crashes:

```bash
# List shared memory segments
ipcs -m

# Remove a specific segment
ipcrm -m <segment_id>
``` 