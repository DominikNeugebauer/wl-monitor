# WL-Monitor

A departure board for Wiener Linien and ÖBB that runs on an ESP32-S3 with a 1.8" ST7735 display.

The device shows the next departures for your chosen lines. Wiener Linien data comes with real-time delays, ÖBB trains show delays when ÖBB has real-time data for them. The data is refreshed every minute and the countdown keeps running in between.

## Setup

Flash the firmware with `pio run -t upload`. The web interface is already included in `src/generated/web_assets.h`.

On first start the display shows a WiFi network called WL-Monitor-XXXX with the password `fahrplan`. Connect your phone to it. The setup page opens by itself. If it does not, open `http://192.168.4.1`.

Pick your home WiFi, enter the password and press connect. The credentials are saved on the device. You can choose your lines right there or later at `http://wl-monitor.local` or the IP address shown on the display.

To reset the WiFi, hold the BOOT button for 5 seconds or use "forget WiFi" in the web interface. If the home WiFi is gone for more than 90 seconds, the setup network turns on again by itself.

## Default lines

S45 from Krottenbachstraße towards Handelskai (ÖBB station 1291902, direction via Oberdöbling 1291904)

S45 from Krottenbachstraße towards Hütteldorf (ÖBB station 1291902, direction via Gersthof 1291801)

38 from Gatterburggasse towards Schottentor (WL RBL 2985)

35A from Obersteinergasse towards Spittelau (WL RBL 1262)

10A from Chimanistraße towards Niederhofstraße (WL RBL 2965)

## Display

The color of the countdown shows the delay. Green means on time, yellow is 1 to 2 minutes late, orange is 3 to 5 minutes late and red is more than 5 minutes late or cancelled. Light blue means there is no real-time data, only the timetable.

The second line of each row shows the delay, or a green dot when the train or bus is on time, followed by the next departures. A pulsing dot means it is leaving now.

The top bar shows the time, the date and the WiFi signal. A red exclamation mark appears when the last fetch failed. The small bar shows the time until the next refresh.

If more than 5 lines are configured, the display pages through them every 8 seconds.

## Data sources

Wiener Linien data comes from the OGD Realtime API. All WL stops are loaded in one request.

ÖBB data comes from the Scotty live ticker at `fahrplan.oebb.at`. If the Wiener Linien API is not reachable, the WL lines are loaded from ÖBB instead.

## Project structure

```
src/main.cpp                    main loop, display output, BOOT button
src/config.h                    pins, WiFi and fetch settings
src/modules/display/ui.*        all screens, drawn into a 160x128 framebuffer
src/modules/net/wifi_manager.*  WiFi connection, setup access point, captive portal
src/modules/net/http_json.*     HTTPS requests with streaming JSON parsing
src/modules/web/web_portal.*    web server and REST API
src/modules/settings/*          saved settings for WiFi, lines and display
src/modules/transit/*           WL and ÖBB requests, parsers, background task
web/index.html                  web interface source
web/stops.txt                   list of WL stops used for the search
tools/embed_web.py              packs web/ into src/generated/web_assets.h
```

After changing anything in `web/`, run `python3 tools/embed_web.py` to update the firmware copy of the web interface.
