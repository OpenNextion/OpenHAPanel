# OpenHAPanel

OpenHAPanel allows users to view and control Home Assistant devices from a dedicated ESP32-S3 screen.

Nextion User Guide and FAQ: https://opennextion.github.io/OpenHAPanel/pages/faq.html

<img width="200" height="300" alt="switch-entity" src="https://github.com/user-attachments/assets/5ab38b75-b4b8-48d8-93c4-624169ee1718" />
<img width="200" height="300" alt="cover-entity" src="https://github.com/user-attachments/assets/f6e59cfc-3f86-466a-8267-137ce49fa986" />
<img width="200" height="300" alt="light-entity" src="https://github.com/user-attachments/assets/6783a575-07e8-4b1c-99d4-31e98382de44" />
<img width="200" height="300" alt="button-entity" src="https://github.com/user-attachments/assets/8febc707-cef0-49b2-871b-ad27f57c55b3" />

## Usage


1. After powering on the device, connect to the device's AP using a smartphone or PC
2. Once the connection is established, open the device's management page in your browser
3. Set a login password (this password will also serve as the device's AP password)
4. Configure Home Assistant authorization on the device's management page
5. Configure the screen layout on the device's management page
6. Now you can view and control your Home Assistant devices on the screen

## Setup

> ESP IDF v5.5.5+ required

1. Init project

    ```
    idf.py set-target esp32s3
    ```

2. Build project

    ```
    idf.py -DDEVICE_MODEL=ONX3248G035 build
    ```

3. Flash project

    ```
    idf.py flash
    ```

> **Note**: source files under `main/assets/`, `main/assets/images/` and
> `main/screen/components/` are collected with `file(GLOB_RECURSE ...)`, which
> has no `CONFIGURE_DEPENDS`. `idf.py build` does not re-run CMake on its own,
> so a file that was **added** or **moved** by a `git pull` may be silently left
> out of the build (link errors for missing symbols). Run `idf.py reconfigure`
> whenever that happens.

`DEVICE_MODEL` is mandatory release identity even though CMake currently
defaults it to `ONX3248G035`. Other values are rejected until their board
support and release assets are added. The device logs the compiled model at
startup and exposes it in `Device.InfoNotify`.

## Web UI

The dashboard UI and the firmware live in the same repository. `web-ui/` is a
Vue 3 + Vite application that is built into a **single self-contained HTML
file**, gzip-compressed, committed, and embedded into the firmware through
`EMBED_FILES` in `components/webserver/CMakeLists.txt`.

Whenever anything under `web-ui/` changes, rebuild the UI and copy the artifact
into the firmware **before** building the firmware:

```
cd web-ui
npm install                              # first time only
VITE_VCONSOLE=0 npm run build            # 0 = release, 1 = debug
cp dist/index.html.gz ../components/webserver/index.html.gz
cd ..
idf.py build
```

- The artifact is `web-ui/dist/index.html.gz`. The path printed by
  `vite-plugin-compression` in the build log is wrong — look in `dist/`.
- `.env` sets `VITE_VCONSOLE=1`, which produces a **debug** build with
  `vconsole` and all `console.*` calls retained (roughly 35% larger). Pass
  `VITE_VCONSOLE=0` on the command line to get the **release** build, as shown
  above. The environment variable overrides `.env`.
- The `cp` step is manual and easy to forget. If you skip it the build still
  succeeds, but the device keeps serving the old page.
- Builds are deterministic: the same source produces a byte-identical `.gz`, so
  comparing `sha256` against the committed file tells you whether a rebuild is
  actually needed.

## Build a single binary

To produce one image containing the bootloader, partition table, OTA data and
the application:

```
idf.py merge-bin -o "$PWD/open-ha-panel-onx3248g035.factory.bin"
```

- `merge-bin` runs an incremental build first, so a separate `idf.py build`
  beforehand is not required.
- Pass an **absolute** path to `-o`: the value is handed to `esptool`, which
  runs with `build/` as its working directory, so a relative path would write
  the file into `build/`.
- Without `-o` the output is `build/merged-binary.bin`.

The merged image contains:

| Offset | Contents |
| --- | --- |
| `0x0` | bootloader |
| `0x8000` | partition table |
| `0x19000` | `ota_data_initial` (blank, boots `ota_0`) |
| `0x20000` | application |

It does **not** contain the `nvs` or `files` (LittleFS) partitions — the device
creates and formats them on first boot. Add `--fill-flash-size 16MB` if you need
an image padded to the full flash size.

> This image is for factory / bulk flashing. **OTA updates use the application
> binary only**: `build/open-ha-panel.bin`.

## Flash a single binary

Write the merged image to offset `0x0`:

```
esptool.py --chip esp32s3 -b 460800 write_flash 0x0 open-ha-panel-onx3248g035.factory.bin
```

Or with the same options `idf.py flash` would use:

```
python -m esptool --chip esp32s3 -b 460800 --before default_reset --after hard_reset \
  write_flash --flash_mode dio --flash_size 16MB --flash_freq 80m \
  0x0 open-ha-panel-onx3248g035.factory.bin
```

To erase the whole flash first:

```
esptool.py --chip esp32s3 erase_flash
```

## Default AP

```
Nextion_XXXX
```

## mDNS Domain

```
nextion.local
nextion-abcd.local # hostname conflict
```

## Flash Firmware

Erase flash:

```
esptool.py --chip esp32s3 \
    --port /dev/ttyUSB0 \
    erase_flash
```

Flash factory bin:

```
esptool.py --chip esp32s3 \
    --port /dev/ttyUSB0 \
    --baud 921600 \
    write_flash --verify \
    0x0 open-ha-panel-onx3248g035.factory.bin
```
