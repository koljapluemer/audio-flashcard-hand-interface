# Run `just` to list recipes.

port := env_var_or_default("PORT", "/dev/ttyACM0")

# Waveshare ESP32-S3-Zero (008 button remote)
fqbn_zero := "esp32:esp32:esp32s3:CDCOnBoot=cdc"
# Waveshare ESP32-S3-ePaper-1.54 (007 flashcards); needs OPI PSRAM / 8MB, see README
fqbn_epaper := "esp32:esp32:esp32s3:PSRAM=opi,PartitionScheme=default_8MB,FlashSize=8M,USBMode=hwcdc,CDCOnBoot=cdc"

app := "code/basic_cordo_android_app"
cordova := "npx cordova"   # project-local, pinned in package.json
apk_dir := app / "platforms/android/app/build/outputs/apk/debug"

default:
    @just --list

# Compile + upload the S3-Zero 3-button BLE remote
flash-zero: (flash "code/scripts/008_esp_s3_zero_ble_buttons" fqbn_zero)

# Compile + upload the e-paper BLE flashcard firmware
flash-epaper: (flash "code/scripts/007_esp_s3_touch_flashcard_ble" fqbn_epaper)

# Compile + upload any sketch folder: just flash <sketch-dir> <fqbn>
flash sketch fqbn:
    arduino-cli compile --fqbn "{{fqbn}}" "{{sketch}}"
    arduino-cli upload  --fqbn "{{fqbn}}" --port "{{port}}" "{{sketch}}"

# Serial monitor (115200 baud)
monitor:
    arduino-cli monitor --port "{{port}}" --config baudrate=115200

# Flash the S3-Zero remote, then attach the serial monitor
flash-zero-monitor: flash-zero monitor

# Check Cordova's Android toolchain (JDK, SDK, Gradle)
apk-doctor:
    cd "{{app}}" && npm install && {{cordova}} requirements android

# Copy data/italian-english-sentences into the app's www/ folder
app-data:
    python3 "{{app}}/build_italian_data.py"

# Build the debug APK and open its folder in the file manager
apk: app-data
    cd "{{app}}" && npm install && {{cordova}} prepare android && {{cordova}} build android
    @echo "APK: {{justfile_directory()}}/{{apk_dir}}/app-debug.apk"
    xdg-open "{{apk_dir}}" >/dev/null 2>&1 &

# Open a shell in the APK output folder (exit to return)
apk-shell:
    cd "{{apk_dir}}" && exec "${SHELL:-bash}"

# Build + install + launch the app on a USB-connected phone
apk-run: app-data
    cd "{{app}}" && npm install && {{cordova}} prepare android && {{cordova}} run android --device
