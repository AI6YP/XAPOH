esp-idf v5.0

Modify `main/secrets.h` for WiFi Auth


```bash
cd ~/work/github/espressif/esp-idf
git pull
git submodule update --init --recursive
./install.sh esp32c6

cd ~/work/github/XAPOH/sw-panel-v3
. ~/work/github/espressif/esp-idf/export.sh

idf.py set-target esp32c6

idf.py build # just build

idf.py -p /dev/ttyUSB0 flash # build & flash

idf.py -p /dev/ttyUSB0 flash monitor # build & flash & monitor -> exit Ctrl+]
```
