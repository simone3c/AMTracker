# AMTracker

ESP32-based project to visualize subway trains in Genoa, Italy in realtime based on real schedules provided by the local transport agency (AMT).

## Description

The device stores internally the timetable which is parsed to create a train list that is periodically scanned to discover trains on the subway. Every active train is then localized and finally displayed on the map. The map contains the 8 stations + some intermediate point between them to better represent the movement of each train in the network. An NTP server is contacted to retrieve the current time to keep the system synched with the real subway.

## TODO list
- [x] WIFI connection
- [x] NTP support
- [x] timetable parser
- [x] train localization
- [ ] train display
- [ ] power management (for longer battery life)
- [ ] UI (refresh rate - wifi connection - ???)
- [ ] PCB
- [ ] ???

## Getting Started

The project is developed on ESP-IDF v5.4.1 and it's tested on an ESP32-WROVER-E.

### Prerequisites

- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/index.html) >= v5.4.1 - Framework for developing, building and flashing code on ESP32.

### Timetable

__TODO__

### Installing and building

Clone this repository
```bash
    > git clone https://github.com/simone3c/AMTracker.git
```
Change WIFI connection parameters (```PSW``` and ```SSID```) inside ```components/my_wifi/my_wifi.c```

Change the NTP server inside ```components/ntp_client/ntp_client.c``` to the optimal one for your position (see [ntppool.org](https://www.ntppool.org/en/))

!!! "Note"
    The first time that you are building and flashing the code, you'll also need to flash the timetable by decommenting the ```FLASH_IN_PROJECT``` flag inside ```main/CMakeLists.txt```. Once it is flashed you can comment the flag back to save time on future flash operations

Build the project
```bash
    > idf.py build
```

## Deployment

To run the code on your ESP32 you need to have necessary components listed in the BOM and you have to replicate the wiring structure showed below.

### Bills of material

- ESP32-WROVER-E
- 74hc595 - shift register
- LEDs (a lot)
- Jumpers (a lot)
- Resistors (a lot)
- __TODO__

### Wiring diagram

__TODO__

### Flash the code

When everything is connected, the last step is to flash the code and finally run the program

```bash
    > idf.py flash
```
!!! "Note"
    The first time that you are building and flashing the code, you'll also need to flash the timetable by decommenting the ```FLASH_IN_PROJECT``` flag inside ```main/CMakeLists.txt```. Once it is flashed you can comment the flag back to save time on future flash operations

## PCB __TODO__

## License

This project is licensed under the [MIT License](LICENSE.md).

## Acknowledgments

Similar project that inspired me are [Traintrackr](https://www.traintrackr.co.uk/) and [Designrules](https://www.designrules.co/)
