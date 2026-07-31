# lms-dial

A small physical remote for [Lyrion Music Server](https://lyrion.org/), built for the M5Stack M5Dial.

The first release will focus on making one player pleasant to control from a desk:

- Turn the dial to change volume
- Short press to play or pause
- Long press to mute or unmute
- Show volume and playback state on the display
- Communicate directly with LMS over Wi-Fi using JSON-RPC

## Project status

Early development. The sketch currently provides a small hardware smoke test for the M5Dial display, encoder, and button.

## Repository layout

```text
firmware/    Arduino firmware for the M5Dial
docs/        Setup, architecture, and user documentation
enclosure/   Source and export files for stands and enclosures
assets/      Logos, icons, screenshots, and other visual assets
```

## Development setup

You will need:

- Arduino IDE 2.x
- ESP32 board support by Espressif Systems
- M5Unified
- M5GFX
- ArduinoJson (used when LMS communication is added)
- A USB-C data cable

Open `firmware/LmsDial/LmsDial.ino` in Arduino IDE, select the M5Dial board and its serial port, then upload the sketch.

## Configuration and secrets

Wi-Fi credentials and local LMS settings must not be committed. When configuration is introduced, copy the provided example file to `secrets.h` and keep your real values only in that ignored file.

## Roadmap

1. Verify display, encoder, and button input
2. Connect to Wi-Fi
3. Send play/pause and volume commands using LMS JSON-RPC
4. Synchronize volume and playback state with LMS
5. Add a polished now-playing interface

## License

This project is licensed under the [MIT License](LICENSE).
