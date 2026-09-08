# Lighthouse browser controller

Control every Group A field using Web Bluetooth, with no build step or external dependencies.

From the repository root, run with Node.js:

```sh
node tools/html/server.mjs
```

Open **http://127.0.0.1:8080** in desktop Chrome or Edge with Bluetooth enabled. Disconnect BLE Scanner first (the firmware supports one connection), click **Connect via Bluetooth**, and select Lighthouse. The page reads the current settings. Edit power, effect, brightness, start/end colors, color mode, rotation period, shift mode and shift period, then click **Apply settings**. **Read device / discard edits** refreshes all fields; **Turn off now** immediately sends off and reloads settings, discarding local edits. Stop the server with Ctrl+C.

Web Bluetooth requires a supported browser, a secure context (HTTPS or localhost), and a user gesture for the device chooser. For a remote/mobile host use HTTPS; a plain HTTP LAN address is insufficient. See [MDN Web Bluetooth](https://developer.mozilla.org/en-US/docs/Web/API/Web_Bluetooth_API). Opening the HTML directly is not supported because it uses JavaScript modules. The server binds only to localhost and serves a fixed list of client assets.

The client follows [BLE protocol v1](../../doc/ble-protocol.md): little-endian float32 x,y and brightness, uint32 periods, and byte enums. It checks the capabilities characteristic and serializes writes with responses. Apply sends changed fields only, turns an active group off during configuration, handles mode/period dependencies, and restores the requested power state last. Writes are separate commands, not atomic; on failure the page attempts to reload the accepted state. A failed apply can leave the lights off. It does not retry writes silently. Readback reports accepted commands, not physical LED output. No notifications are available; use Read device to refresh.

Color pickers convert sRGB to CIE x,y using the firmware matrices; they do not change brightness. Black has no chromaticity and maps to D65 white. Use brightness zero or power off for darkness. Numeric coordinates preserve device float32 values unless edited. The six-LED preview shows approximate static A→B→A gradient placement, not an effect animation or a calibrated brightness simulation. An unused endpoint may read as invalid coordinates on older startup configurations; choose a valid end color before enabling Gradient.

Run the protocol tests with:

```sh
node --test tools/html/protocol.test.mjs
```

Physical connect/read/write/reconnect testing needs the flashed board and a Web Bluetooth browser. The client never uses Group B. Settings survive disconnect but reset on firmware reboot.
