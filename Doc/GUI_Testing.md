# GUI Testing with Xvfb and xdotool

## Overview

This document describes the automated GUI testing setup for L2Trader using Xvfb (X Virtual FrameBuffer) and xdotool for simulating user interactions in CI/CD environments.

## Background

As L2Trader has moved into integration phase, unit tests alone are insufficient to validate the application's behavior. High-level GUI integration tests are needed to ensure the application can start, respond to user input, and exit cleanly.

## Testing Approach

### Tools Used

1. **Xvfb (X Virtual FrameBuffer)**: Provides a virtual X11 display server that runs in memory without requiring a physical display. This allows GUI applications to run in headless CI/CD environments.

2. **xdotool**: Simulates keyboard and mouse input to X11 windows, enabling automated interaction with the GUI application.

3. **x11-utils**: Provides utilities like `xdpyinfo` for querying and verifying X11 display information.

### Test Workflow

The GUI testing is integrated into the main build workflow (`.github/workflows/build.yml`) as a separate job that runs after the build job completes.

#### Workflow Structure

1. **Build Job**: 
   - Builds the GUI version of L2Trader
   - Uploads the compiled executable as an artifact

2. **Test-GUI Job**:
   - Downloads the GUI executable artifact from the build job
   - Runs the GUI test script (Xvfb, xdotool, and x11-utils are pre-installed in the container)

#### Test Script Steps

1. **Start Xvfb**: Launch a virtual X11 display on `:99` with 1024x768 resolution and 24-bit color depth
2. **Verify Display**: Use `xdpyinfo` to confirm the virtual display is operational
3. **Launch Application**: Start L2Trader in the background
4. **Wait for Startup**: Allow 5 seconds for the application to initialize and create its window
5. **Find Window**: Use `xdotool search` to locate the L2Trader window by class name
6. **Send Ctrl+Q**: Simulate the Quit keyboard shortcut to the application window
7. **Wait for Exit**: Monitor the application process for up to 10 seconds
8. **Verify Exit Code**: Check that the application exited cleanly (exit code 0 or 143)
9. **Cleanup**: Terminate the Xvfb process

### Exit Code Interpretation

- **0**: Clean exit - the application shut down normally
- **143**: SIGTERM received - the application was terminated cleanly
- **Other codes**: Unexpected exit - may indicate a crash or error

Both 0 and 143 are considered successful test outcomes.

## Local Testing

A convenience script is provided for local testing: `Utils/test-gui.sh`

### Prerequisites

```bash
sudo apt-get install xvfb xdotool x11-utils
```

### Running Locally

```bash
# Build the GUI version first
mkdir -p build/GUI
cmake -S . -B build/GUI -DCMAKE_BUILD_TYPE=Debug -DENABLE_GUI=ON -DBUILD_TESTS=OFF
cmake --build build/GUI -j$(nproc)

# Run the test script
./Utils/test-gui.sh
```

The script will:
- Check for required dependencies
- Verify the L2Trader executable exists
- Run the same test sequence as the CI workflow
- Report success or failure

## Limitations and Future Improvements

### Current Limitations

1. **Single Test Case**: Currently only tests application startup and Ctrl+Q quit
2. **No Visual Verification**: Cannot verify UI rendering or layout
3. **Limited Interaction**: Only simulates one keyboard shortcut
4. **No Error Detection**: Cannot detect UI errors or warnings in dialogs

### Future Enhancements

Potential improvements to the GUI testing framework:

1. **Additional Test Cases**:
   - Test main window controls and buttons
   - Verify tab switching functionality
   - Test dialog opening/closing
   - Simulate stock symbol input and search

2. **Screenshot Capture**:
   - Take screenshots at various test stages
   - Compare screenshots for visual regression testing
   - Capture error dialogs

3. **Enhanced Interaction**:
   - Mouse click simulation
   - Text input to various UI fields
   - Menu navigation testing
   - Keyboard shortcut coverage

4. **Error Detection**:
   - Monitor application logs during tests
   - Detect Qt warnings and errors
   - Verify expected UI elements exist

5. **Test Framework**:
   - Create a reusable test framework
   - Support multiple test scenarios
   - Generate test reports with artifacts

## Troubleshooting

### Window Not Found

If `xdotool search` cannot find the window:
- Increase the startup wait time (currently 5 seconds)
- Verify the window class name matches "L2Trader"
- Check application logs for startup errors
- Ensure Qt platform plugin is available (likely `offscreen` or `xcb`)

### Application Crashes on Startup

If the application crashes immediately:
- Check for missing Qt libraries or plugins
- Verify all runtime dependencies are installed
- Review application logs for error messages
- Test the executable can run in a regular X11 session first

### Xvfb Fails to Start

If Xvfb cannot initialize:
- Check for port conflicts on display :99
- Verify Xvfb package is installed correctly
- Try a different display number (e.g., :100)

## Implementation Details

### Workflow Configuration

Location: `.github/workflows/build.yml`

Key configuration:
```yaml
test-gui:
  runs-on: ubuntu-24.04
  needs: build  # Depends on successful build
  container:
    image: ghcr.io/${{ vars.USERNAME }}/l2trader-builder:noble
```

### Artifact Management

Build artifacts are stored temporarily (1 day retention) to enable testing:
```yaml
- name: Upload GUI executable artifact
  uses: actions/upload-artifact@v4
  with:
    name: l2trader-gui-${{ github.sha }}
    path: build/GUI/Src/L2Trader
    retention-days: 1
```

### Test Script Location

The test logic is embedded directly in the workflow YAML for simplicity. For more complex test scenarios, consider moving to separate test scripts in the repository.

## References

- [Xvfb Documentation](https://www.x.org/releases/X11R7.6/doc/man/man1/Xvfb.1.xhtml)
- [xdotool Documentation](https://github.com/jordansissel/xdotool)
- [GitHub Actions Artifacts](https://docs.github.com/en/actions/using-workflows/storing-workflow-data-as-artifacts)
