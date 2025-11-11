# Debian Packaging for L2Trader

This directory contains the Debian packaging metadata for L2Trader.

## Package Format

This package uses the **native Debian format (3.0 native)** as specified in `source/format`.

## Building the Package

### Prerequisites

Install the required build dependencies:

```bash
sudo apt-get install debhelper-compat qt6-base-dev qt6-charts-dev \
                     qt6-webengine-dev libqt6sql6-sqlite \
                     qtkeychain-qt6-dev libsecret-1-dev
```

### Build Process

From the root directory of the repository:

```bash
dpkg-buildpackage -us -uc -b
```

This will create two `.deb` packages in the parent directory:
- `l2trader_<version>_<arch>.deb` - Main application package
- `l2trader-tests_<version>_<arch>.deb` - Unit tests package

## Packages

### l2trader

The main application package that includes:
- **Binaries:**
  - `/usr/bin/l2trader` - Main GUI application
  - `/usr/bin/l2trader-recorder` - Recorder application
- **Application Resources:**
  - `/usr/share/applications/l2trader.desktop` - Desktop entry
  - `/usr/share/pixmaps/l2trader.png` - Application icon
- **Documentation:**
  - `/usr/share/doc/l2trader/examples/` - Example configuration files

### l2trader-tests

Unit test package that includes:
- `/usr/lib/l2trader/tests/test_barcache` - BarCache unit tests
- `/usr/lib/l2trader/tests/test_runupdetector` - RunUpDetector unit tests
- `/usr/lib/l2trader/tests/test_tradestationclient` - TSClient unit tests

## Key Features

### QtKeychain Integration

The Debian package build **enables QtKeychain support**, which is disabled by default in the development `.pro` files. This allows secure credential storage using the system keyring:

- Uses `libsecret` on Linux
- Falls back to obfuscated QSettings if QtKeychain is not available
- Properly declares `qtkeychain-qt6-dev` as a build dependency
- Properly declares `libqt6keychain1` as a runtime dependency

### Qt6 Application Integration

The package properly designates this as a Qt6 application through:

- Desktop file in `/usr/share/applications/`
- Application icon in `/usr/share/pixmaps/`
- Proper categorization in the application menu (Office > Finance)
- Complete Qt6 module dependencies (core, gui, widgets, network, sql, charts, webengine)

### Installation and Deinstallation

Standard Debian package management:

```bash
# Install
sudo dpkg -i l2trader_*.deb
sudo apt-get install -f  # Fix any missing dependencies

# Or use apt
sudo apt install ./l2trader_*.deb

# Remove
sudo apt remove l2trader

# Purge (removes configuration files)
sudo apt purge l2trader
```

## CI/CD Integration

The `.github/workflows/build.yml` workflow automatically builds the Debian package on:
- Push to main branch
- Pull requests to main branch
- Manual workflow dispatch

The built packages are uploaded as GitHub Actions artifacts.

## Files

- `changelog` - Package version history
- `compat` - Debhelper compatibility level (13)
- `control` - Package metadata and dependencies
- `copyright` - License information (MIT)
- `rules` - Build instructions (Makefile)
- `l2trader.desktop` - Desktop entry for application menu
- `source/format` - Package format specification (3.0 native)

## Testing

After installation, verify the package:

```bash
# Check installed files
dpkg -L l2trader

# Run the application
l2trader

# Run unit tests (if l2trader-tests is installed)
/usr/lib/l2trader/tests/test_barcache --stock-csv=/usr/share/doc/l2trader/examples/nasdaq_screener.csv
/usr/lib/l2trader/tests/test_runupdetector
/usr/lib/l2trader/tests/test_tradestationclient
```

## Maintenance

To update the package version:

1. Update the version in `debian/changelog`
2. Add a new changelog entry with `dch -i` or manually
3. Rebuild the package

## Notes

- The package uses `${shlibs:Depends}` in the control file to automatically detect shared library dependencies
- Build artifacts are excluded from git via `.gitignore`
- The FMPClient test is intentionally excluded as it's disabled in the CI workflow
