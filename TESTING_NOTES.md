# Testing Notes for --recorded-data-dir Option

## Feature Description
This feature adds a new command-line option `--recorded-data-dir` to the Recorder application, allowing users to specify a custom directory for recorded live data (bars and market depth).

## Use Case
This is particularly useful for Raspberry Pi deployments where users want to write data to an external USB drive (e.g., `/mnt/ssd`) instead of the SD card to avoid wear and improve performance.

## Manual Testing Steps

### 1. Build the application
```bash
mkdir build
cd build
cmake -S .. -B . -DCMAKE_BUILD_TYPE=Debug -DENABLE_GUI=ON -DBUILD_TESTS=ON
cmake --build . --parallel
```

### 2. Test with default behavior (no option specified)
```bash
./src/Recorder --stock-csv=../Example_Config/nasdaq_screener_mini.csv
# Data should be written to ~/.cache/Recorder/RecordedLiveData/
```

### 3. Test with custom directory
```bash
mkdir -p /tmp/test_recorder_data
./src/Recorder --recorded-data-dir=/tmp/test_recorder_data --stock-csv=../Example_Config/nasdaq_screener_mini.csv
# Data should be written to /tmp/test_recorder_data/RecordedLiveData/
```

### 4. Verify directory structure
After running the Recorder for a few seconds (Ctrl+C to stop), check:
```bash
ls -la /tmp/test_recorder_data/RecordedLiveData/
ls -la /tmp/test_recorder_data/RecordedLiveData/Bars/
ls -la /tmp/test_recorder_data/RecordedLiveData/MarketDepthQuotes/
```

Expected structure:
```
/tmp/test_recorder_data/
└── RecordedLiveData/
    ├── Bars/
    │   └── RecordedLiveBars_YYYY-MM-DD.db
    └── MarketDepthQuotes/
        └── RecordedLiveMarketDepthQuotes_YYYY-MM-DD.db
```

### 5. Run integration tests
```bash
cd build
ctest -R test_recorder_integration -V
```

## Expected Log Output

When using the custom directory, you should see in the log:
```
Recorded data directory set to: /tmp/test_recorder_data
Using custom recorded data directory: /tmp/test_recorder_data
Recorded data folder: /tmp/test_recorder_data/RecordedLiveData
```

When using the default behavior:
```
Using default cache location: /home/user/.cache/Recorder
Recorded data folder: /home/user/.cache/Recorder/RecordedLiveData
```

## Raspberry Pi Deployment Example

For a Raspberry Pi with a USB drive mounted at `/mnt/ssd`:

1. Mount the USB drive:
```bash
sudo mkdir -p /mnt/ssd
sudo mount /dev/sda1 /mnt/ssd
```

2. Update the systemd service file `/lib/systemd/system/l2trader-recorder.service`:
```ini
ExecStart=/usr/bin/l2trader-recorder \
  --stock-csv=/usr/share/doc/l2trader/examples/nasdaq_screener.csv \
  --recorded-data-dir=/mnt/ssd
```

3. Ensure the l2trader user has write permissions:
```bash
sudo mkdir -p /mnt/ssd/RecordedLiveData
sudo chown l2trader:l2trader /mnt/ssd/RecordedLiveData
```

4. Reload and restart the service:
```bash
sudo systemctl daemon-reload
sudo systemctl restart l2trader-recorder.service
```

## Backward Compatibility
The change is fully backward compatible. If `--recorded-data-dir` is not specified, the Recorder will use the default cache location as before.

## Notes
- The directory specified will have `/RecordedLiveData` appended to it
- The directory will be created automatically if it doesn't exist
- The user running the Recorder must have write permissions to the specified directory
