# Debian Packaging for OpenTraderPlatform

This directory contains the Debian packaging metadata for OpenTraderPlatform.

## Package Format

This package uses the **native Debian format (3.0 native)** as specified in `source/format`.

## Building the Package

### Prerequisites

Install the required build dependencies:

```bash
sudo apt-get install debhelper build-essential cmake ninja-build python3 qt6-base-dev \
                     qtkeychain-qt6-dev libqt6sql6-sqlite libprotobuf-dev protobuf-compiler \
                     libssl-dev libzstd-dev
```

`ccache` is optional. If installed, CMake detects and uses it automatically.

### Build Process

From the root directory of the repository:

```bash
dpkg-buildpackage -us -uc -b
```

This will create Debian packages in the parent directory, including:
- `opentraderplatform_<version>_<arch>.deb` - Main application package
- `opentraderplatform-strategy-sdk_<version>_<arch>.deb` - Runtime library for external strategy executables
- `opentraderplatform-strategy-sdk-dev_<version>_<arch>.deb` - Headers, Protobuf schema, and CMake package files for strategy development

## Packages

### opentraderplatform

The main application package that includes:
- **Binaries:**
  - `/usr/bin/opentraderplatform` - Main GUI application
- **Application Resources:**
  - `/usr/share/applications/opentraderplatform.desktop` - Desktop entry
  - `/usr/share/pixmaps/opentraderplatform.png` - Application icon

### opentraderplatform-strategy-sdk

Runtime package for out-of-process strategy executables:
- `/usr/lib/<multiarch>/libOpenTraderPlatformStrategySDK.so.*` - Shared strategy SDK runtime

### opentraderplatform-strategy-sdk-dev

Development package for building external strategies:
- `/usr/include/OpenTraderPlatform/StrategySDK/` - Public C++ SDK headers
- `/usr/include/OpenTraderPlatform/StrategyProtocol/` - Generated Protobuf headers
- `/usr/share/opentraderplatform/proto/` - Canonical `.proto` files
- `/usr/lib/<multiarch>/cmake/OpenTraderPlatformStrategySDK/` - `find_package()` metadata

## Key Features

### Credential Storage

Qt6Keychain is a required build and runtime dependency. Credentials are stored in the native OS keyring with insecure fallback disabled. A working, unlocked desktop keyring is required to save credentials. Old obfuscated settings are discarded without migration; users must re-enter credentials.

### Qt6 Application Integration

The package properly designates this as a Qt6 application through:

- Desktop file in `/usr/share/applications/`
- Application icon in `/usr/share/pixmaps/`
- Proper categorization in the application menu (Office > Finance)
- Qt6 module dependencies (core, gui, widgets, network, SQL, print support)

### Installation and Deinstallation

Standard Debian package management:

```bash
# Install
sudo dpkg -i opentraderplatform_*.deb
sudo apt-get install -f  # Fix any missing dependencies

# Or use apt
sudo apt install ./opentraderplatform_*.deb

# Remove
sudo apt remove opentraderplatform

# Purge (removes configuration files)
sudo apt purge opentraderplatform
```

### Credential Management

The current build stores sensitive credentials in the obfuscated QSettings fallback, not the system keyring:

**Stored credentials:**
- TradeStation API client ID and secret
- Access tokens, refresh tokens, and ID tokens

When the package is purged, application configuration files are removed. The current build does not create keyring entries.

## CI/CD Integration

The `.github/workflows/post-merge.yml` workflow builds Debian packages after pushes to the `main` branch.

The built packages are uploaded as GitHub Actions artifacts.

## Files

- `changelog` - Package version history
- `control` - Package metadata and dependencies (includes debhelper-compat level)
- `copyright` - GPL-3.0-or-later project license and third-party license notices
- `rules` - Build instructions (Makefile)
- `opentraderplatform.desktop` - Desktop entry for application menu
- `source/format` - Package format specification (3.0 native)

## Verification

After installation, verify the package and launch the application:

```bash
# Check installed files
dpkg -L opentraderplatform

# Run the application
opentraderplatform

```

## Maintenance

To update the package version:

1. Update the version in `debian/changelog`
2. Add a new changelog entry with `dch -i` or manually
3. Rebuild the package

## Notes

- The package uses `${shlibs:Depends}` in the control file to automatically detect shared library dependencies
- Build artifacts are excluded from git via `.gitignore`
