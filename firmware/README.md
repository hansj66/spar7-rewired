| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C6 | ESP32-H2 | ESP32-P4 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | -------- | -------- | -------- |

# Spool controller

## Building

1. Download [espressif installer ](https://dl.espressif.com/dl/esp-idf/?idf=4.4)
2. Configure environemnt variable IDF_PATH to point to espressif installation folder
3. If using visual studio code: Install espressif plugin

```
idf.py build flash monitor
```


## Folder contents

```
├── CMakeLists.txt
├── pytest_main.py      Python script used for automated testing
├── main
│   ├── CMakeLists.txt
│   └── main.c
└── README.md                  This is the file you are currently reading
```


