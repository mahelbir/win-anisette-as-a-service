# Anisette as a Service
This project extracts Apple's Anisette data from iTunes and iCloud for Windows on demand and exposes it as a simple HTTP api.

# Credits
The actual extraction code is taken from https://github.com/NyaMisty/alt-anisette-server

# Why?
The original project used node.js to expose the gathered anisette data as an HTTP api. Since the "api" is very simple, this project uses Winsockets to send the response directly.