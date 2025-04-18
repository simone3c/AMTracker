# AMTracker

ESP32-based project to visualize subway trains in Genoa, Italy in realtime based on real schedules provided by the local transport agency.

## Description

The device stores internally the timetable which is parsed to create a train list that is periodically scanned to discover trains on the subway. Every active train is then localized and finally displayed on the map. The map contains the 8 stations + some intermediate point between them to better represent the movement of each train in the network. An NTP server is periodically contacted to retrieve the current time.

## TODO list
- [x] WIFI connection
- [x] NTP support
- [x] timetable parser
- [x] train localization
- [x] train display
- [ ] power management (for longer battery life)
- [ ] UI (power button - errors - refresh rate - wifi connection - ???)
- [ ] PCB
- [ ] ???

## Getting Started

The project is developed on ESP-IDF v5.4.1 and it's tested on an ESP32-WROVER-E.

### Prerequisites

- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/index.html) >= v5.4.1 - Framework for developing, building and flashing code on ESP32.

### Timetable

The timetable are stored inside ```data/stop_times.txt``` and can be freely downloaded from the local transport agency's website. To transform them in the correct format used in this project you need to run the python script ```data/timetable.py```:
```bash
    > python3 timetable.py stop_times.txt
```
This operation will produce the file ```stop_times_fixed.csv``` which should be moved inside ```spiffs_root```

### Installing and building

Clone this repository
```bash
    > git clone https://github.com/simone3c/AMTracker.git
```
Create the file ```components/my_wifi/wifi_credentials.c``` and set the macros ```PSW``` and ```SSID``` in order to connect to your access point 

Change the NTP server inside ```components/ntp_client/ntp_client.c``` to the optimal one for your position (see [ntppool.org](https://www.ntppool.org/en/))

!!! "Note"
    The first time that you are building and flashing the code, you'll also need to flash the timetable by decommenting the ```FLASH_IN_PROJECT``` flag inside ```main/CMakeLists.txt```. Once it is flashed you can comment it to save time on future build operations

Build the project
```bash
    > idf.py build
```

## Deployment

To run the code on your ESP32 you need to have necessary components listed in the BOM and you have to replicate the wiring structure showed below.

### Bills of material

__TODO__

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
