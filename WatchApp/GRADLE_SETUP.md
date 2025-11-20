# Gradle Wrapper Setup

The `gradle-wrapper.jar` file is intentionally not included in the repository for security and space reasons.

## Generating the Gradle Wrapper JAR

When you first try to build the project, you may need to generate the gradle-wrapper.jar file. There are two ways to do this:

### Option 1: Using an existing Gradle installation

If you have Gradle installed on your system:

```bash
cd WatchApp
gradle wrapper --gradle-version 8.0
```

This will download and set up the complete Gradle wrapper, including the `gradle-wrapper.jar` file.

### Option 2: Using Android Studio

1. Open the WatchApp project in Android Studio
2. When prompted, click "Sync Project with Gradle Files"
3. Android Studio will automatically download the Gradle wrapper

### Option 3: Manual Download (if needed)

If the above methods don't work, you can manually download the wrapper jar:

1. Download from: https://services.gradle.org/distributions/gradle-8.0-bin.zip
2. Extract the zip file
3. Copy `gradle-8.0/lib/plugins/gradle-wrapper-*.jar` to `WatchApp/gradle/wrapper/gradle-wrapper.jar`

## After Setup

Once the gradle-wrapper.jar is in place, you can use the gradlew script:

```bash
./gradlew assembleDebug
```

The wrapper will handle downloading the correct Gradle version automatically.
