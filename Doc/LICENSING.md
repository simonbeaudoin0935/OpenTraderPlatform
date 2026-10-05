# Licensing and Distribution

## Project License

OpenTraderPlatform's original code, public strategy SDK, examples, tests, and
documentation are licensed under **GPL-3.0-or-later**, except where an explicit
third-party notice specifies otherwise. See [LICENSE](../LICENSE) for the grant
and [COPYING](../COPYING) for the complete GNU GPL version 3.

This replaces the project's previous MIT-only declaration. Bundled QCustomPlot
is GPL-3.0-or-later and is compiled into the application; declaring the whole
combined application MIT-only did not account for that dependency.

Third-party material is not relicensed by this change. Existing copyright,
license, and attribution notices must be preserved. Previously distributed
copies of original project code under MIT do not lose their existing license.
Relicensing also does not override any rights held by third-party contributors.

## Third-Party Components

| Component | License / licensing options | Distribution notes |
|-----------|-----------------------------|--------------------|
| Bundled QCustomPlot 2.1.1 | GPL-3.0-or-later | Copyright 2011-2022 Emanuel Eichhammer; preserve the notices in [qcustomplot.h](../Lib/QCustomPlot/qcustomplot.h) and [qcustomplot.cpp](../Lib/QCustomPlot/qcustomplot.cpp). |
| databento-cpp | Apache-2.0 | Retain the [upstream license](../Lib/Databento/LICENSE) and any applicable attribution notices. |
| Qt6 | Module-dependent LGPL/GPL or commercial terms | Comply with the terms for the exact Qt modules and binaries distributed; the project's GPL does not replace Qt's license. |
| Qt6Keychain | BSD-2-Clause, with additional notices for upstream build files | Preserve the upstream license notices when redistributing the library. |
| Protocol Buffers | BSD-3-Clause, plus dependency-specific notices | Retain the notices for the exact library and dependencies distributed. |
| OpenSSL, Zstandard, and databento-cpp's fetched dependencies | Version- and component-specific upstream licenses | Inventory the exact versions shipped and include applicable notices and license texts. |

This table identifies the direct dependencies, not a complete license inventory
of every transitive component in a release artifact. Before publishing binary
packages or container images, review their actual contents, including statically
linked dependencies, and retain all required upstream licenses and notices.

## Source and Binary Releases

- Source releases must include the project license, complete GPL text, original
  third-party notices, and the dependency sources required for the release.
- When distributing binaries, provide the complete corresponding source under
  an applicable GPL section 6 option. Publishing matching source alongside the
  binaries is a straightforward approach for downloadable releases.
- Corresponding source includes relevant build/install scripts and modifications,
  not only the application's own source files. A Git submodule pointer alone is
  not a substitute for fulfilling the chosen source-distribution requirements.
- Keep the source for each released binary available as required by the GPL.
  Do not rewrite away source revisions needed to reproduce published binaries.
- Debian packages carry the project and bundled-component notices through
  [debian/copyright](../debian/copyright); system dependencies retain their
  separately packaged license information.

## Private Strategies

This change does not modify or relicense the separate `OTP_Strategies` repository.
The GPL permits private use and private modifications without a public-release
requirement. However, distributing a strategy linked to the GPL-licensed public
SDK may impose GPL obligations on the combined work. Running a strategy in a
separate process does not automatically resolve that question.

Review the licensing arrangement before distributing proprietary strategy
binaries; a separate SDK license or exception would be a deliberate future
decision, not something granted by this document.

## Market Data and Service Credentials

The software license grants no rights to redistribute TradeStation or Databento
market data, credentials, or third-party branding. Those remain subject to their
respective agreements and rights.

These notes explain the project's licensing intent and release checklist; they
are not a substitute for legal advice where ownership or distribution rights
are uncertain.
