# L2Trader Watch App

A companion Wear OS application for Samsung Galaxy Watch that displays L2Trader information on your wrist.

## Overview

This is a standalone Wear OS application written in Java that can run on Samsung Galaxy Watches and other Wear OS devices. Currently, it displays a simple "Hello World" message as a foundation for future trading data integration.

## Features

- ✅ Hello World display on watch face
- 🎯 Optimized for Wear OS devices
- 📱 Compatible with Galaxy Watch series
- 🔋 Minimal battery consumption

## Requirements

- **Android Studio**: Arctic Fox (2020.3.1) or later
- **Java Development Kit (JDK)**: 8 or higher
- **Gradle**: 8.0+ (included via wrapper)
- **Target Device**: Wear OS 3.0+ (API level 30+)
- **Samsung Galaxy Watch**: Watch 4, 5, 6 or newer

## Project Structure

```
WatchApp/
├── app/
│   ├── src/main/
│   │   ├── java/com/simonbeaudoin/l2trader/watchapp/
│   │   │   └── MainActivity.java          # Main activity with Hello World
│   │   ├── res/
│   │   │   ├── layout/
│   │   │   │   └── activity_main.xml      # Watch UI layout
│   │   │   └── values/
│   │   │       ├── strings.xml            # String resources
│   │   │       └── dimens.xml             # Dimension values
│   │   └── AndroidManifest.xml            # App manifest
│   ├── build.gradle                        # App-level build config
│   └── proguard-rules.pro                  # ProGuard rules
├── gradle/
│   └── wrapper/                            # Gradle wrapper files
├── build.gradle                            # Project-level build config
├── settings.gradle                         # Gradle settings
├── gradle.properties                       # Gradle properties
└── gradlew                                 # Gradle wrapper script (Linux/Mac)
```

## Building the App

### Using Android Studio

1. **Open the project**:
   ```bash
   # From Android Studio, select File > Open
   # Navigate to: /path/to/L2Trader/WatchApp
   ```

2. **Sync Gradle**:
   - Android Studio will automatically prompt to sync Gradle
   - Or click: File > Sync Project with Gradle Files

3. **Build the APK**:
   - Select: Build > Build Bundle(s) / APK(s) > Build APK(s)
   - Or use the build toolbar button

### Using Command Line

1. **Navigate to WatchApp directory**:
   ```bash
   cd WatchApp
   ```

2. **Build the app**:
   ```bash
   ./gradlew assembleDebug
   ```

3. **Output location**:
   ```
   app/build/outputs/apk/debug/app-debug.apk
   ```

## Installation

### Via Android Studio

1. Connect your Galaxy Watch via USB or enable wireless debugging
2. Ensure developer options are enabled on the watch
3. Click the "Run" button (▶️) in Android Studio
4. Select your watch device from the list

### Via ADB (Android Debug Bridge)

1. **Enable ADB debugging** on your Galaxy Watch:
   - Settings > About watch > Software > Tap "Software version" 5 times
   - Go back to Settings > Developer options > Enable ADB debugging

2. **Connect watch** via USB or WiFi:
   ```bash
   # For WiFi (replace with your watch IP):
   adb connect <watch-ip-address>:5555
   ```

3. **Install the APK**:
   ```bash
   adb install app/build/outputs/apk/debug/app-debug.apk
   ```

## Running the App

1. On your Galaxy Watch, open the app drawer
2. Look for "L2Trader Watch" icon
3. Tap to launch
4. You should see "Hello from L2Trader Watch!" displayed on the screen

## Development

### Customizing the Hello World Message

Edit `MainActivity.java`:
```java
mTextView.setText("Your custom message here!");
```

Edit `strings.xml`:
```xml
<string name="hello_world">Your custom message here!</string>
```

### Adding New Features

The app is structured to easily add new features:

1. **Add new activities** in `java/com/simonbeaudoin/l2trader/watchapp/`
2. **Add new layouts** in `res/layout/`
3. **Add new resources** in `res/values/`
4. **Update dependencies** in `app/build.gradle`

### Key Files

- **MainActivity.java**: Entry point of the application
- **activity_main.xml**: Main UI layout with BoxInsetLayout for round watch faces
- **AndroidManifest.xml**: Declares Wear OS compatibility and permissions

## Troubleshooting

### Gradle Build Issues

```bash
# Clean build
./gradlew clean

# Rebuild
./gradlew assembleDebug --refresh-dependencies
```

### Cannot Find Watch Device

1. Check USB connection or WiFi pairing
2. Verify ADB debugging is enabled on watch
3. Run: `adb devices` to see connected devices

### App Crashes on Launch

1. Check LogCat in Android Studio
2. Ensure target SDK matches watch OS version
3. Verify all required permissions are granted

## Future Enhancements

Planned features for integration with L2Trader:

- 📊 Real-time stock price display
- 📈 Market alerts and notifications
- 💰 Portfolio balance updates
- ⏰ Trading session timers
- 📱 Bi-directional communication with main L2Trader app

## Technical Details

### Wear OS Specifics

- **Standalone App**: Can run independently without phone companion
- **BoxInsetLayout**: Automatically adapts to round and square watch faces
- **Material Design**: Follows Wear OS design guidelines
- **Battery Optimization**: Uses efficient UI updates

### Dependencies

- `com.google.android.gms:play-services-wearable:18.1.0` - Wear OS services
- `androidx.wear:wear:1.3.0` - Wear UI components
- `androidx.appcompat:appcompat:1.6.1` - Android compatibility
- `com.google.android.support:wearable:2.9.0` - Additional Wear support

## Contributing

When adding features to the Watch app:

1. Follow Android/Java coding conventions
2. Test on both round and square watch faces
3. Optimize for battery consumption
4. Document new features in this README

## License

This Watch app is part of the L2Trader project and follows the same license terms.

## Support

For issues specific to the Watch app, please include:
- Watch model and Wear OS version
- Android Studio version
- Build error logs or crash reports
- Steps to reproduce any issues

---

**Note**: This is a development/boilerplate version. The app currently displays a Hello World message and serves as a foundation for future trading features.
