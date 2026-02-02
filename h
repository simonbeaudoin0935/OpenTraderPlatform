* [33m8249237[m[33m ([m[1;36mHEAD[m[33m -> [m[1;32mfeature/strategy-plugin-system[m[33m)[m Fix log auto-scroll to stay disabled when user scrolls up
* [33m9852267[m Increase log font size to 10px
* [33md11f001[m Increase log display font size from 7px to 9px
* [33mab5efdf[m Match start/stop button states and colors to strategy status
* [33m32cfd03[m Change HistoricalBarsStrategy fetch interval back to 2 seconds
* [33mabbfac8[m Auto-scroll log display to bottom unless user scrolls up
* [33m3ac73ea[m[33m ([m[1;31morigin/feature/strategy-plugin-system[m[33m)[m Skip weekends in HistoricalBarsStrategy bar fetching
* [33m792d42d[m Remove 5-day limit and store fetched bars in vector
* [33m32d05d1[m Change HistoricalBarsStrategy to fetch days every 500ms
* [33m662fad1[m Add start/stop buttons to StrategyCard
* [33m4b435bd[m Fuse strategy tile and panel into unified StrategyCard widget
* [33mda88c08[m Homogenize tile styling and improve field presentation
* [33me47fb74[m Optimize tile layout: reduce tile height, remove selection highlighting
* [33m86003e4[m Fix strategy loading: retrieve dialog result and call StrategyManager.loadStrategy()
* [33m842a5a0[m Refactor strategies UI: horizontal tile layout with embedded panels
* [33m644b24c[m Implement full-day cache warming logic in BarCache
* [33m4209e9c[m Add graceful signal handling for strategy crashes (SIGSEGV, SIGABRT, SIGTERM)
* [33me5c5e5b[m Fix strategy tile grid layout and status preservation
* [33m67dab0b[m Add strategy failure handling and colorized button styling
* [33m6376c90[m Fix strategy thread context: move strategy object to thread, name threads, fix signal handler
* [33m1db722c[m Add CrashTestStrategy - test strategy that intentionally segfaults
* [33ma22ca86[m Phase 4.9b: Bundle strategy tile fields in single unified contour
* [33m5b2e505[m Phase 4.9: Unified strategy tile styling - single green contour
* [33m6cd11af[m Phase 4.8: Defer strategy auto-start - require user to click Start button
* [33m6d2c1e9[m Fix: Logs display filter defaults to 'All' instead of 'Debug only'
* [33mc75d9ff[m Fix: Connect StrategySDK logging to StrategyLogger
* [33mbdcabe4[m Phase 4.6: Implement Strategy Control Buttons (Start/Stop)
* [33m87a23b2[m Phase 4.5: Integrate Strategy Logs Viewer into Details Panel
* [33m2a9102d[m Add post-build install command for ExampleStrategy plugin
* [33m71daf23[m Phase 4.4: Implement Strategy Details Panel with real-time updates
* [33m0d41f59[m Fix: Build L2TraderStrategy as shared library for plugin loading
* [33m6883895[m Add getStrategyAPIVersion() export to ExampleStrategy
* [33m58decf3[m Fix: Expand tilde (~) in strategy plugin paths
* [33md497747[m Add debug logging to strategy load flow
* [33m9bb85c9[m Fix: Remove unused refreshConfigList() slot from StrategyLoadDialog
* [33mb1e8be4[m Phase 4.3: Implement Strategy Load Dialog
* [33m9dfb252[m Create ExampleStrategy plugin with full callback implementations
* [33m554d904[m Phase 4.2: Implement tile view enhancements with real-time CPU/memory stats
* [33m89fd9b5[m Swap Logging and Strategies tab order
* [33mbd6d9bd[m Remove Settings tab and move StrategiesTab to second position
* [33me2030cd[m Revert "Move StrategiesTab to second tab and remove ShortcutsTab"
* [33mdcb5cad[m Move StrategiesTab to second tab and remove ShortcutsTab
* [33m09a6130[m Phase 4.1: Create StrategiesTab widget with grid and details panel
* [33mea40b46[m Phase 3.6 - Graceful Strategy Shutdown
* [33mc9e467f[m Phase 3.5 - Per-Thread SIGSEGV Handler for Strategy Crashes
* [33md2d35d8[m Phase 3.4 - Strategy Logging System
* [33mbdfa1d1[m Phase 3.3 - Complete order routing implementation
* [33m3b53da1[m Phase 3.2 - Connect MainAlgo signals to StrategyManager
* [33m207ef5d[m Phase 2.4 - Strategy Discovery & Registry
* [33mfa1e7d3[m Phase 2.3 - Strategy Configuration Loading
* [33m080e70f[m Phase 2.2: Implement thread-safe signal delivery with StrategyCallbackAdapter
* [33ma124c13[m StrategyBase: Assert SDK pointer validity in log() method
* [33m272bd66[m Phase 2.1: Rename Strategy to StrategyBase, add lifecycle hooks and common helpers
* [33mb440ba1[m Phase 1.5: Implement order validation, QFuture return, and order routing callbacks
* [33m639ff9c[m replace for assert
* [33m05db5c5[m Use logging macros
* [33m4970cb5[m Optimize MainAlgo future continuation thread handling
* [33mba0219f[m Refactor MainAlgo to use unique_ptr for balance polling timer and improve logging
* [33m0ac1ad9[m Phase 1.4: Implement MainAlgo dispatcher for strategy order routing
* [33m0f3dde4[m Phase 1.3: Implement StrategyLoader and StrategyManager
* [33mdc649ed[m Phase 1.1-1.2: Add Strategy plugin system foundation
*   [33m7c1a41f[m[33m ([m[1;31morigin/main[m[33m, [m[1;31morigin/HEAD[m[33m, [m[1;32mmain[m[33m)[m Merge pull request #248 from simonbeaudoin0935/copilot/remove-runupdetector-and-selection-criteria
[32m|[m[33m\[m  
[32m|[m * [33m9c70141[m[33m ([m[1;31morigin/copilot/remove-runupdetector-and-selection-criteria[m[33m)[m Update documentation to remove RunUpDetector references
[32m|[m * [33m942650c[m Apply code formatting after cleanup
[32m|[m * [33m3c685e5[m Remove RunUpDetector and selection_criteria functionality
[32m|[m * [33m399f46a[m Initial plan
[32m|[m[32m/[m  
*   [33m0bf1b0a[m Merge pull request #247 from simonbeaudoin0935/fix
[34m|[m[35m\[m  
[34m|[m * [33m73d21f9[m fix backgrounds
[34m|[m[34m/[m  
*   [33me4139fa[m Merge pull request #246 from simonbeaudoin0935/copilot/fix-price-chart-visual-bug
[36m|[m[1;31m\[m  
[36m|[m * [33m1e0ab8a[m[33m ([m[1;31morigin/copilot/fix-price-chart-visual-bug[m[33m)[m wip
[36m|[m * [33m598987e[m wip
[36m|[m * [33md3bfd9b[m Fix after-market purple rectangle to cover 20:00 bar
[36m|[m * [33m2587148[m Initial plan
[36m|[m[36m/[m  
*   [33maae36ae[m Merge pull request #245 from simonbeaudoin0935/copilot/fix-auth-token-validation
[1;32m|[m[1;33m\[m  
[1;32m|[m * [33m87d1a08[m[33m ([m[1;31morigin/copilot/fix-auth-token-validation[m[33m, [m[1;32mcopilot/fix-auth-token-validation[m[33m)[m Reduce screenshot retention days from 7 to 2 in GUI test workflow
[1;32m|[m * [33m60da838[m Fix Auth token scope validation to match actual API scopes
[1;32m|[m * [33md45fd6d[m Initial plan
[1;32m|[m[1;32m/[m  
*   [33m091ee0f[m Merge pull request #220 from simonbeaudoin0935/copilot/gracefully-exit-app
[1;34m|[m[1;35m\[m  
[1;34m|[m * [33m2197f42[m[33m ([m[1;31morigin/copilot/gracefully-exit-app[m[33m)[m Add debug logging for empty credentials in AuthToken and ClientToken
[1;34m|[m * [33m9ad5f6c[m Add step to print config files before running GUI tests
[1;34m|[m * [33mf693a4c[m Refactor setup test credentials step to pass secrets as arguments and remove credential checks
[1;34m|[m * [33m4e7bbd4[m Add setup for test credentials in CI workflow
[1;34m|[m * [33m1876aea[m Fix QFuture assertion in Stream::onReplyFinished during shutdown
[1;34m|[m * [33mc9d901e[m Fix QFuture context assertion by using explicit shutdown flag
[1;34m|[m * [33m9320d3a[m Replace nullptr checks with assertions and fix QFuture context crash
[1;34m|[m * [33m0f8a768[m Fix thread affinity assertion in MainApp shutdown
[1;34m|[m * [33mc0aed83[m Add documentation for graceful shutdown mechanism
[1;34m|[m * [33m9849dc1[m Add destroyInstance() methods to singletons for proper cleanup
[1;34m|[m * [33me55b6ea[m Implement graceful shutdown mechanism with MainApp singleton
[1;34m|[m[1;34m/[m  
* [33m3a7078c[m Add condition to skip builds for draft pull requests
*   [33m0bf7fa2[m Merge pull request #243 from simonbeaudoin0935/copilot/centralize-constants-file
[1;36m|[m[31m\[m  
[1;36m|[m * [33ma416840[m Centralize constants into CONSTANTS.h with organized namespaces
[1;36m|[m * [33mcdf70f1[m Initial plan
[1;36m|[m[1;36m/[m  
* [33md8dc8bc[m Implement logging settings persistence in LoggingTab
*   [33mb63fdb6[m Merge pull request #241 from simonbeaudoin0935/copilot/organize-bar-cache-files
[32m|[m[33m\[m  
[32m|[m * [33maf7c23f[m[33m ([m[1;31morigin/copilot/organize-bar-cache-files[m[33m)[m Use ASSUME_TRUE for critical directory creation check
[32m|[m * [33mc3d986c[m Extract Bars subdirectory name to constant for better maintainability
[32m|[m * [33ma521966[m Reorganize bar cache files into dedicated Bars subdirectory
[32m|[m * [33med4cd39[m Initial plan
[32m|[m[32m/[m  
*   [33m03435d4[m Merge pull request #239 from simonbeaudoin0935/copilot/add-gui-testing-with-xvfb
[34m|[m[35m\[m  
[34m|[m * [33mdb4d9b6[m Move GUI test deps to Docker image and add screenshot capture
[34m|[m * [33m0c3732d[m Fix: Remove window activation to avoid _NET_ACTIVE_WINDOW error
[34m|[m * [33m58c020c[m Security: Update artifact actions to patched versions
[34m|[m * [33m3338f3e[m Add explicit permissions to workflow jobs for security
[34m|[m * [33mce0e6d9[m Improve error handling and messaging in GUI test workflow
[34m|[m * [33m3ab9087[m Fix: Add GUI testing dependencies and improve process wait logic
[34m|[m * [33m63bcb43[m Remove redundant dependency installation and add documentation
[34m|[m * [33me374ca1[m Add GUI testing workflow with Xvfb and xdotool
[34m|[m * [33m648a084[m Initial plan
[34m|[m[34m/[m  
* [33m728e885[m Improve semaphore management in checkForMissingBars and clearSymbol methods
* [33m4cd3b18[m Initial plan
*   [33mbdc10cf[m Merge pull request #230 from simonbeaudoin0935/copilot/change-accounts-info-location
[36m|[m[1;31m\[m  
[36m|[m * [33m423e0db[m[33m ([m[1;31morigin/copilot/change-accounts-info-location[m[33m)[m Remove logDisplay widget from bottom horizontal layout
[36m|[m * [33maa4b664[m Remove logDisplay widget and improve account info button icon
[36m|[m * [33m50b81d6[m Remove unused onAccountSelectionChanged slot per code review
[36m|[m * [33m3dd38e9[m Add account info button with popup instead of logDisplay widget
[36m|[m * [33m12e0e1b[m Initial plan
[36m|[m[36m/[m  
*   [33mc11e284[m Merge pull request #222 from simonbeaudoin0935/copilot/fix-stock-selection-crash
[1;32m|[m[1;33m\[m  
[1;32m|[m * [33m9c2deb6[m[33m ([m[1;31morigin/copilot/fix-stock-selection-crash[m[33m)[m fix format
[1;32m|[m * [33m1adb5f9[m Fix crash in Stream destructor: Close streams before deleteLater()
[1;32m|[m * [33me16152b[m Fix segfault: Capture stream by value in closeStream lambda
[1;32m|[m * [33mdb2a802[m Address code review - improve cleanup order and add validation
[1;32m|[m * [33m9422395[m Fix segfault: Use QPointer and deleteLater() instead of manual delete
[1;32m|[m * [33m16e706c[m Add comment explaining BarCache destructor cleanup
[1;32m|[m * [33m2e7d334[m Address code review - fix potential race condition in pointer handling
[1;32m|[m * [33m51d956e[m Fix resource leak when switching stocks - clean up old StockInstruments
[1;32m|[m * [33m5a494ee[m Fix app crash when changing stock symbols - handle empty indexToBar
[1;32m|[m * [33mca84e9d[m Initial plan
[1;32m|[m[1;32m/[m  
* [33m1c55d2f[m fix-index
*   [33ma167bd9[m Merge pull request #216 from simonbeaudoin0935/copilot/tackle-architecture-point-1-1
[1;34m|[m[1;35m\[m  
[1;34m|[m * [33m0c32204[m[33m ([m[1;31morigin/copilot/tackle-architecture-point-1-1[m[33m)[m Update documentation: remove completed point 1.1 and add threading pattern rule
[1;34m|[m * [33m199cc6e[m Fix inconsistent thread ownership pattern in TSClient - use stack allocation
[1;34m|[m[1;34m/[m  
* [33mbac5ae7[m fix: correct date extraction logic in populateAvailableReplayDays
*   [33mcf6928b[m Merge pull request #210 from simonbeaudoin0935/copilot/fix-segmentation-fault
[1;36m|[m[31m\[m  
[1;36m|[m * [33m59119f7[m[33m ([m[1;31morigin/copilot/fix-segmentation-fault[m[33m)[m fix bug happening at 23:59
[1;36m|[m * [33m9b2bb31[m wip
[1;36m|[m * [33m3c10a19[m Fix segfault by changing getBars return type from unique_ptr to shared_ptr
[1;36m|[m * [33me00dc33[m wip
[1;36m|[m * [33m5936101[m wip
[1;36m|[m * [33m08ec3f4[m wip
[1;36m|[m * [33mbecd2fe[m Add error just to give content to the promise
[1;36m|[m * [33m36d4dc4[m add tui debug task
[1;36m|[m * [33m2f5ad3e[m Debug: Comment out TSClient continuation to isolate segfault cause
[1;36m|[m * [33m535e5b6[m Remove .then() continuation on storeBarsInDatabase to avoid threading issues
[1;36m|[m * [33mf417f3c[m Fix time boundary issue in bar request - use current time truncated to minute
[1;36m|[m * [33m8300e76[m Fix segmentation faults in TUI mode
[1;36m|[m * [33m71f1ac4[m Add null checks for m_fetchedDayBars to prevent segfault
[1;36m|[m * [33m4c8018d[m Remove unused algoLogFile code causing segfault
[1;36m|[m[1;36m/[m  
* [33mbd6a8a9[m Refactor getBars method to round up current time for accurate bar requests
* [33medd7b34[m Pass credentials as script arguments instead of env vars
* [33m5e35d8b[m Trigger workflow run to test credentials verification step
* [33m2139a89[m Add credentials check step to copilot-setup-steps workflow
* [33m6f3d080[m add credentials
* [33m9fc3793[m ye
* [33m7719ed9[m Refactor TUIFrontend to manage last displayed stock and enhance display functionality
*   [33m4c79407[m Merge pull request #207 from simonbeaudoin0935/copilot/switch-to-assume-macros
[32m|[m[33m\[m  
[32m|[m *   [33mb9ba8e0[m[33m ([m[1;31morigin/copilot/switch-to-assume-macros[m[33m)[m Merge branch 'main' into copilot/switch-to-assume-macros
[32m|[m [34m|[m[32m\[m  
[32m|[m [34m|[m[32m/[m  
[32m|[m[32m/[m[34m|[m   
* [34m|[m [33maa4c180[m Refactor BarCache getBars method for improved error handling and logging; add timeToIndex and indexToTime methods to BarCache header
* [34m|[m [33meb8681f[m fix
[35m|[m * [33m3450de5[m Switch all Q_ASSERT to ASSUME macros and format code
[35m|[m * [33m07767cf[m Fix compilation errors from Q_ASSERT to ASSUME conversion
[35m|[m * [33m11b7af3[m Replace all remaining Q_ASSERT with ASSUME macros in all source files
[35m|[m * [33m443e9b3[m Replace Q_ASSERT with ASSUME macros in GUI tabs and OrderWindow
[35m|[m * [33m0f3f939[m Replace Q_ASSERT with ASSUME macros in TSClient, PlaceOrder, SecureStorage, databases, and Algo classes
[35m|[m * [33mb3c3075[m Initial plan
[35m|[m[35m/[m  
*   [33m41da15b[m Merge pull request #170 from simonbeaudoin0935/copilot/suggest-architecture-improvements
[36m|[m[1;31m\[m  
[36m|[m * [33m0b7f7c0[m[33m ([m[1;31morigin/copilot/suggest-architecture-improvements[m[33m)[m Fix clang-format issues: remove trailing whitespace
[36m|[m *   [33m903ef54[m Merge branch 'main' into copilot/suggest-architecture-improvements
[36m|[m [1;32m|[m[36m\[m  
[36m|[m [1;32m|[m[36m/[m  
[36m|[m[36m/[m[1;32m|[m   
* [1;32m|[m [33mdaf5266[m update copilot instructions
* [1;32m|[m [33ma93184e[m Improve error handling in BarCache::storeBarInCache: log warnings for duplicate bars and critical errors for uninitialized slots
* [1;32m|[m [33m495b5a9[m Enhance CI workflows: add clang-format check and improve ccache setup
* [1;32m|[m [33m317f236[m Refactor CI workflow: integrate clang-format into build process and remove standalone clang-format workflow
* [1;32m|[m   [33m9a60a8a[m Merge pull request #204 from simonbeaudoin0935/copilot/fix-reopening-position-widget
[1;34m|[m[1;35m\[m [1;32m\[m  
[1;34m|[m * [1;32m|[m [33mc0c509c[m[33m ([m[1;31morigin/copilot/fix-reopening-position-widget[m[33m)[m fix formatter
[1;34m|[m * [1;32m|[m   [33m6f0e0c9[m Merge branch 'main' into copilot/fix-reopening-position-widget
[1;34m|[m [1;36m|[m[1;34m\[m [1;32m\[m  
[1;34m|[m [1;36m|[m[1;34m/[m [1;32m/[m  
[1;34m|[m[1;34m/[m[1;36m|[m [1;32m|[m   
* [1;36m|[m [1;32m|[m   [33mde3148b[m Merge pull request #201 from simonbeaudoin0935/copilot/convert-to-smart-pointers
[32m|[m[33m\[m [1;36m\[m [1;32m\[m  
[32m|[m * [1;36m|[m [1;32m|[m [33m789f8f2[m[33m ([m[1;31morigin/copilot/convert-to-smart-pointers[m[33m)[m Fix clang-format: remove trailing whitespace from markdown files
[32m|[m * [1;36m|[m [1;32m|[m [33m393416f[m Apply composition preference: convert MainAlgo receivers to Qt parent-child ownership
[32m|[m * [1;36m|[m [1;32m|[m [33m074244d[m Add composition over pointers preference to guidelines
[32m|[m * [1;36m|[m [1;32m|[m [33mfdfed0e[m Add comprehensive smart pointer usage documentation
[32m|[m * [1;36m|[m [1;32m|[m [33med3ecd3[m Convert MainAlgo receiver pointers to smart pointers and fix StockInstruments parent
[32m|[m * [1;36m|[m [1;32m|[m [33m1e651d2[m Convert raw pointers to smart pointers in key classes
[32m|[m[32m/[m [1;36m/[m [1;32m/[m  
* [1;36m|[m [1;32m|[m   [33md438c15[m Merge pull request #206 from simonbeaudoin0935/copilot/make-tui-viable
[34m|[m[35m\[m [1;36m\[m [1;32m\[m  
[34m|[m * [1;36m|[m [1;32m|[m [33mba952d3[m[33m ([m[1;31morigin/copilot/make-tui-viable[m[33m)[m Prevent terminal scrolling from erasing TUI display
[34m|[m * [1;36m|[m [1;32m|[m [33m86b72db[m Fix signal-slot connections to properly route orders and positions to TUI
[34m|[m * [1;36m|[m [1;32m|[m [33mb7d2dc8[m Redirect TUI logs to stderr to avoid interfering with ncurses display
[34m|[m * [1;36m|[m [1;32m|[m [33mfecfb09[m Add TUI configuration, build, and run tasks to VSCode
[34m|[m * [1;36m|[m [1;32m|[m [33mb437b53[m Remove _codeql_detected_source_root symlink
[34m|[m * [1;36m|[m [1;32m|[m [33m3bbcc53[m Remove accidentally committed _codeql_build_dir artifacts
[34m|[m * [1;36m|[m [1;32m|[m [33m28a1eb3[m Implement functional ncurses-based TUI with order/position monitoring
[34m|[m * [1;36m|[m [1;32m|[m [33mf9f387b[m Address code review feedback - fix format specifiers and floating point comparison
[34m|[m * [1;36m|[m [1;32m|[m [33mb3b81aa[m Add comprehensive TUI documentation and update README
[34m|[m * [1;36m|[m [1;32m|[m [33m44f77fd[m Apply clang-format to TUI implementation
[34m|[m * [1;36m|[m [1;32m|[m [33m8523492[m Implement ncurses-based TUI with order/position display and keyboard shortcuts
[34m|[m * [1;36m|[m [1;32m|[m [33mef36b77[m Fix TUI build errors - CMakeLists typos, conditional compilation, shadowing
[34m|[m[34m/[m [1;36m/[m [1;32m/[m  
* [1;36m|[m [1;32m|[m [33m2ad3efe[m Enhance database analysis script to check for 'stockTicker' column in the table
* [1;36m|[m [1;32m|[m [33m20dc910[m Add entries to .gitignore for build artifacts
[35m|[m * [1;32m|[m [33m17c3dfc[m Add CodeQL artifacts to .gitignore
[35m|[m * [1;32m|[m [33m497d02f[m Complete fix for position widget reopening issue
[35m|[m * [1;32m|[m [33m8b82b54[m Improve quantity comparison to use numerical values
[35m|[m * [1;32m|[m [33m95aaa45[m Fix position widget to reuse 0-quantity rows when reopening positions
[35m|[m * [1;32m|[m [33m065f319[m Initial plan
[35m|[m[35m/[m [1;32m/[m  
* [1;32m|[m   [33m5771121[m Merge pull request #202 from simonbeaudoin0935/fix-assert
[36m|[m[1;31m\[m [1;32m\[m  
[36m|[m * [1;32m|[m [33mca657a9[m[33m ([m[1;31morigin/fix-assert[m[33m)[m Refactor Order class: update member variable names to use 'm_' prefix and adjust constructor and getter methods accordingly
[36m|[m * [1;32m|[m [33mc15a963[m Refactor Order class: update member variable names to use 'm_' prefix and adjust constructor and getter methods accordingly
[36m|[m[36m/[m [1;32m/[m  
* [1;32m|[m   [33m481ca69[m Merge pull request #199 from simonbeaudoin0935/copilot/make-oauth-tui-compatible
[1;32m|[m[1;33m\[m [1;32m\[m  
[1;32m|[m * [1;32m|[m [33mf159264[m Delete Src/Misc/Logging_generated.h
[1;32m|[m * [1;32m|[m [33me93b90c[m Rename HeadlessAuthHandler to TUIAuthHandler and AuthWindow to GUIAuthHandler
[1;32m|[m * [1;32m|[m [33m5f55294[m Format LiveStreamDB.cpp with clang-format
[1;32m|[m * [1;32m|[m [33m85a8afe[m Address code review feedback: fix typos and lambda capture
[1;32m|[m * [1;32m|[m [33mf0db49a[m Document GUI vs TUI authentication support
[1;32m|[m * [1;32m|[m [33m7088590[m Fix CMakeLists typo and format code with clang-format
[1;32m|[m * [1;32m|[m [33m5c466cb[m Add base AuthHandler and HeadlessAuthHandler for TUI support
[1;32m|[m[1;32m/[m [1;32m/[m  
* [1;32m|[m [33m3c69754[m Fix path case for Logging_generated.h in .gitignore
* [1;32m|[m   [33ma416701[m Merge pull request #197 from simonbeaudoin0935/copilot/centralise-sql-queries
[1;34m|[m[1;35m\[m [1;32m\[m  
[1;34m|[m * [1;32m|[m [33m546acb8[m Delete Src/Misc/Logging_generated.h
[1;34m|[m * [1;32m|[m [33md3579fb[m Reorganize SQL queries into separate headers in Src/SQL/ folder
[1;34m|[m * [1;32m|[m [33mcffced1[m Apply clang-format to SqlQueries.h
[1;34m|[m * [1;32m|[m [33ma21de8b[m Centralize all SQL queries to Src/Misc/SqlQueries.h
[1;34m|[m * [1;32m|[m [33med409c6[m Initial plan
[1;34m|[m[1;34m/[m [1;32m/[m  
* [1;32m|[m   [33me560257[m Merge pull request #196 from simonbeaudoin0935/copilot/fix-linting-errors
[1;36m|[m[31m\[m [1;32m\[m  
[1;36m|[m * [1;32m|[m [33m6f55ad3[m Fix cppcheck warnings and additional clang-tidy issues
[1;36m|[m * [1;32m|[m [33mbba74d1[m Fix clang-tidy warnings and configure linting workflow
[1;36m|[m * [1;32m|[m [33m8e962e7[m Initial commit: Add .clang-tidy configuration to fix linting setup
[1;36m|[m * [1;32m|[m [33m5458a62[m Initial plan
[1;36m|[m[1;36m/[m [1;32m/[m  
* [1;32m|[m [33m1e03cc3[m Add clang-tidy to Dockerfile for amd64 noble
* [1;32m|[m [33mf1b45b0[m Remove error handling from clang-tidy command
* [1;32m|[m [33m79db831[m Update lint workflow to use 'Src/' directory
* [1;32m|[m [33m782647d[m Add cppcheck to Dockerfile for amd64 noble
* [1;32m|[m [33m61a9aca[m [WIP] Fix clang-format job by formatting required file (#194)
* [1;32m|[m [33m2103ead[m Update build instructions for clang-formatter
* [1;32m|[m [33m770c5a0[m Update clang-format.yml
* [1;32m|[m [33md5b00da[m Fix compilation errors from new -Werror warnings (#192)
* [1;32m|[m [33m8726679[m Modify build command and fix section titles
* [1;32m|[m [33m91500a7[m Set BUILD_TESTS to OFF in CMake
* [1;32m|[m [33md71e16e[m[33m ([m[1;31morigin/copilot/fix-compilation-errors-werror[m[33m)[m [WIP] Check all source files for trailing spaces (#184)
* [1;32m|[m [33m6dd10fe[m Create copilot-setup-steps.yml workflow and update build documentation (#182)
* [1;32m|[m   [33mf0c81bc[m Merge pull request #180 from simonbeaudoin0935/clang-format-all-files
[32m|[m[33m\[m [1;32m\[m  
[32m|[m * [1;32m|[m [33m4e7fcc1[m[33m ([m[1;31morigin/clang-format-all-files[m[33m)[m Automatically remove trailing whitespace from staged C++ files in pre-commit hook
[32m|[m * [1;32m|[m [33m5fcfd2a[m clang-format the whole repo
[32m|[m * [1;32m|[m [33m248f43e[m Refactor formatting configuration and update workflow files for consistency
[32m|[m * [1;32m|[m [33m5647a4d[m Replace Uncrustify with clang-format in Dockerfiles for consistent code formatting
[32m|[m * [1;32m|[m [33m8562d99[m Update pre-commit hook to use clang-format instead of Uncrustify for C++ file formatting
[32m|[m[32m/[m [1;32m/[m  
* [1;32m|[m   [33madcaf87[m Merge pull request #178 from simonbeaudoin0935/uncrust
[34m|[m[35m\[m [1;32m\[m  
[34m|[m * [1;32m|[m [33me08f987[m[33m ([m[1;31morigin/uncrust[m[33m)[m Enhance pre-commit hook and action: add YAML trailing whitespace checks and update descriptions
[34m|[m * [1;32m|[m [33me84b292[m Replace Uncrustify with clang-format: add action and workflow for code formatting checks
[34m|[m * [1;32m|[m [33mf0f8ad6[m Add .clang-format configuration file for consistent code formatting
[34m|[m * [1;32m|[m [33mf4a251b[m Refactor Uncrustify configuration: enhance comments for clarity and maintain consistency in formatting rules
[34m|[m * [1;32m|[m [33m719a88a[m added config for class init list and lambdas
[34m|[m[34m/[m [1;32m/[m  
* [1;32m|[m   [33mccc327d[m Merge pull request #177 from simonbeaudoin0935/build-workflow-overhaul
[36m|[m[1;31m\[m [1;32m\[m  
[36m|[m * [1;32m|[m [33m630a074[m[33m ([m[1;31morigin/build-workflow-overhaul[m[33m)[m Refactor GitHub Actions workflows: remove Uncrustify step from build, update lint job settings, and create dedicated Uncrustify workflow
[36m|[m * [1;32m|[m [33m11f8a9b[m overhaul the build workflow
[36m|[m[36m/[m [1;32m/[m  
* [1;32m|[m [33me0b38a7[m Add uncristify and remove charts and webengine dependancy
* [1;32m|[m [33mac21df9[m Remove obsolete GitHub Actions workflow for issue summarization
* [1;32m|[m [33mc7cbe0a[m Cleanup GitHub Actions workflows by removing obsolete FMPClient test job and refining path triggers for container build
* [1;32m|[m   [33m94e0e13[m Merge pull request #176 from simonbeaudoin0935/copilot/add-ubsan-and-extra-warnings
[1;32m|[m[1;33m\[m [1;32m\[m  
[1;32m|[m * [1;32m|[m [33m52b0e83[m[33m ([m[1;31morigin/copilot/add-ubsan-and-extra-warnings[m[33m)[m Refactor CMake configuration to enhance compiler warnings and enable test builds
[1;32m|[m * [1;32m|[m [33mebd3c5d[m Add VSCode task configurations for UBSan and ASan builds
[1;32m|[m * [1;32m|[m [33me088334[m Add workflow permissions to UBSan jobs for security
[1;32m|[m * [1;32m|[m [33m7162c85[m Improve CMake configuration with modern practices and compiler checks
[1;32m|[m * [1;32m|[m [33mfb0d1ba[m Refine compiler warnings to be Qt-compatible
[1;32m|[m * [1;32m|[m [33mced68fd[m Update README with sanitizer build instructions
[1;32m|[m * [1;32m|[m [33m03417c9[m Add UBSan, extra compiler warnings, and code quality documentation
[1;32m|[m[1;32m/[m [1;32m/[m  
* [1;32m|[m [33m0a7f9b9[m Refactor order status handling in Order constructor to remove TODO and improve clarity
* [1;32m|[m [33mae8a01c[m Refactor BarCache stream parameters for clarity and update splitIntoTradingDayRanges signature
* [1;32m|[m [33m7fde915[m Refactor enum conversion functions in QtEnum for improved error handling and logging
* [1;32m|[m [33m0ef44eb[m Remove stack trace printing for fatal messages in coloredMessageOutput function
* [1;32m|[m [33mca0c530[m Enhance assertion macros in Assume.h for better debugging and runtime checks
[1;33m|[m * [33m697eb27[m Use Q_DISABLE_COPY_MOVE for TSClient singleton and update documentation
[1;33m|[m * [33m66916d9[m Add architecture improvement documentation and implement Q_DISABLE_COPY for singletons
[1;33m|[m[1;33m/[m  
*   [33mb44dac5[m Merge pull request #172 from simonbeaudoin0935/copilot/add-uncrustify-pre-commit-hook
[1;34m|[m[1;35m\[m  
[1;34m|[m * [33me8d07c9[m Add CI/CD formatting check job for uncrustify
[1;34m|[m * [33m44aca81[m Separate essential and optional apt packages in README
[1;34m|[m * [33m1438434[m Fix pre-commit hook issues from code review
[1;34m|[m * [33m60c418f[m Update Utils README with git hooks documentation
[1;34m|[m * [33m6b798ae[m Add Uncrustify pre-commit hook for code formatting
[1;34m|[m * [33mf825ce3[m Remove test file
[1;34m|[m * [33mc9e6a1e[m Test commit with properly formatted code
[1;34m|[m[1;34m/[m  
*   [33m544760a[m Merge pull request #162 from simonbeaudoin0935/fix-ur-mom
[1;36m|[m[31m\[m  
[1;36m|[m *   [33mfc0698b[m[33m ([m[1;31morigin/fix-ur-mom[m[33m)[m Merge branch 'main' into fix-ur-mom
[1;36m|[m [32m|[m[1;36m\[m  
[1;36m|[m [32m|[m[1;36m/[m  
[1;36m|[m[1;36m/[m[32m|[m   
* [32m|[m   [33mf0964bc[m Merge pull request #166 from simonbeaudoin0935/copilot/add-filled-time-column
[34m|[m[35m\[m [32m\[m  
[34m|[m * [32m|[m [33m7c86044[m[33m ([m[1;31morigin/copilot/add-filled-time-column[m[33m)[m Enable bidirectional widget collapsing in QSplitter
[34m|[m * [32m|[m [33m37f2f7e[m Add horizontal QSplitter to bottom panel for resizable widgets (balance, position, order, etc.)
[34m|[m * [32m|[m [33m02c5617[m Enable horizontal scrollbar in OrderWindow to prevent exceeding screen width
[34m|[m * [32m|[m [33m354d754[m remove bar limit chart
[34m|[m * [32m|[m [33m80c9ab8[m Log error message for unimplemented AdvancedOptions parsing in Order constructor
[34m|[m * [32m|[m [33me581901[m Refactor logging color definitions for consistency and clarity
[34m|[m * [32m|[m [33md7daea6[m Refactor OrdersDatabase to use singleton pattern for single persistent connection
[34m|[m * [32m|[m [33m5711fc8[m Fix OrdersDatabase connection name conflicts by using unique connection names per instance
[34m|[m * [32m|[m [33mab87dc8[m Add separate Ack Latency and Fill Latency columns to track order acknowledgment and fill times
[34m|[m * [32m|[m [33mb5246ff[m Change Filled Time column to show order placement to fill latency in seconds/milliseconds
[34m|[m * [32m|[m [33m8c31a86[m fix bad time for current day
[34m|[m * [32m|[m [33m2aed96c[m Refactor OrdersReceiver to use MainApp time and improve OrdersDatabase order insertion
[34m|[m * [32m|[m [33m5295cca[m Add Orders Database management UI to Cache tab with statistics and clear functionality
[34m|[m * [32m|[m [33m42dc289[m wip
[34m|[m * [32m|[m [33m7647ee3[m Add Q_ENUM to Error enum in TSClient and implement toString for enums in Logging
[34m|[m * [32m|[m [33mf92d4f9[m wip
[34m|[m * [32m|[m [33md00167b[m Fix deprecated QVariant constructor usage for Qt6 compatibility
[34m|[m * [32m|[m [33m8ac6c78[m Add clarifying comment about minimal JSON storage design decision
[34m|[m * [32m|[m [33m0ae3ac6[m Address code review feedback: extract helper methods, fix duplication, restore timestamps from database
[34m|[m * [32m|[m [33m87e1c78[m Fix includes and database binding issues in OrdersDatabase and OrdersReceiver
[34m|[m * [32m|[m [33m976a557[m Add OrdersDatabase and integrate with OrdersReceiver, add filled time column to OrderWindow
[34m|[m * [32m|[m [33m0839073[m Initial plan
[34m|[m[34m/[m [32m/[m  
* [32m|[m [33m3943fe9[m Replace manual stacktrace with C++23 stacktrace API
[35m|[m * [33m38d2c28[m fix check
[35m|[m * [33mfef0dfd[m Optimize Bar retrieval and add assertions for missing data handling
[35m|[m * [33m52645a1[m Clarify pre-market and after-hours time ranges in StockPriceChart background drawing
[35m|[m * [33m9424326[m Add volume auto-rescale feature to StockPriceChart toolbar
[35m|[m * [33m9ed0dc2[m Refactor PositionsReceiver: Move createPositionsStream call to constructor body
[35m|[m * [33mc70e161[m Enhance StockPriceChart: Implement dynamic rescaling of volume axis based on visible data range
[35m|[m * [33m5d769c1[m Refactor StockPriceChart: Optimize background drawing logic and track drawn dates
[35m|[m * [33mc679c6f[m Refactor StockPriceChart: Improve logging and code formatting for clarity
[35m|[m * [33m3e2a1d7[m fix order of aquisition
[35m|[m * [33m415262e[m wip
[35m|[m * [33m6eb1a41[m Refactor BarCache and GUI components to use shared_ptr for improved memory management and consistency
[35m|[m * [33me45e9f4[m Refactor BarCache and DatabaseThread to use shared_ptr for better memory management
[35m|[m * [33mdd1b98a[m wip
[35m|[m * [33me2b465b[m Refactor BarCache and DatabaseThread for improved database handling and asynchronous operations
[35m|[m * [33ma4315a6[m wip
[35m|[m * [33m07b1647[m Refactor TSClient::isCleanedUp() to focus on open streams and remove pending requests tracking
[35m|[m[35m/[m  
*   [33mf72f58c[m Merge pull request #160 from simonbeaudoin0935/copilot/create-shortcut-tab
[36m|[m[1;31m\[m  
[36m|[m * [33mae06352[m[33m ([m[1;31morigin/copilot/create-shortcut-tab[m[33m)[m Add cancel all confirmation setting and fix QHash operator[] issue
[36m|[m * [33m10327e0[m Fix build error: use QHash instead of QMap for Order storage
[36m|[m * [33mf660a08[m Filter cancel all orders to only cancel orders in cancellable states
[36m|[m * [33mceca270[m Fix order execution shortcuts to update button appearance and add cancel all orders shortcut
[36m|[m * [33md13d68d[m Add result popup toggle to OrderEntryWidget settings menu
[36m|[m * [33maacd670[m Add settings dropdown menu to OrderEntryWidget with order confirmation toggle
[36m|[m * [33m1dc2acb[m Add 4 new shortcuts for order execution (Buy, Sell, BuyToCover, SellToCover)
[36m|[m * [33m67e1c74[m Fix Qt::UniqueConnection assertion: remove from qApp connection
[36m|[m * [33me14c113[m Fix syntax error: remove duplicate closing braces in ShortcutsTab.cpp
[36m|[m * [33m5259e52[m Fix build error: remove Qt::UniqueConnection from lambda connections
[36m|[m * [33m3effb47[m Add detailed UI design documentation with mockup
[36m|[m * [33me2ad37c[m Add comprehensive implementation summary documentation
[36m|[m * [33m4a1f745[m Fix code review issues: include path, lambda captures, and resetToDefault return value
[36m|[m * [33me6f8b08[m Fix code review findings: correct include path and remove unnecessary mutable keywords
[36m|[m * [33m02cad2d[m Add Qt::UniqueConnection to all signal/slot connections
[36m|[m * [33m4d256d4[m Fix signal/slot signature for shortcut changes and add documentation
[36m|[m * [33m1b7e624[m Add ShortcutSettings singleton and ShortcutsTab implementation
[36m|[m[36m/[m  
*   [33m6baba98[m Merge pull request #161 from simonbeaudoin0935/copilot/remove-embedded-browser-dependency
[1;32m|[m[1;33m\[m  
[1;32m|[m * [33ma0d4783[m[33m ([m[1;31morigin/copilot/remove-embedded-browser-dependency[m[33m)[m Fix cancellation
[1;32m|[m * [33m3d0ead8[m Improve logging consistency in onAuthFinished
[1;32m|[m * [33m66684d7[m Fix crash: Set m_authToken and m_apiKey after successful OAuth
[1;32m|[m * [33m948a223[m Replace QWebEngineView with system browser for OAuth authentication
[1;32m|[m * [33m9a1de8b[m Fix typo: Authentification -> Authentication
[1;32m|[m * [33mfd45932[m Add comprehensive OAuth browser migration documentation
[1;32m|[m * [33m7863ae4[m Replace QWebEngineView with system browser for OAuth
[1;32m|[m * [33m62b69ce[m Initial plan
[1;32m|[m[1;32m/[m  
* [33m8370b10[m Rewrite Thread_Relations_TSClient.md with accurate architecture details
* [33mc3886c4[m Refactor getBars to use QPromise directly for improved clarity and performance
*   [33m6f86df9[m Merge pull request #157 from simonbeaudoin0935/feat/get-rid-of-exceptions
[1;34m|[m[1;35m\[m  
[1;34m|[m * [33meee243d[m[33m ([m[1;31morigin/feat/get-rid-of-exceptions[m[33m)[m Remove unnecessary include of Logging.moc from Logging.cpp
[1;34m|[m * [33m173fb2b[m wip
[1;34m|[m * [33m37340b7[m Refactor error handling in async requests to use std::expected for better clarity and control
[1;34m|[m * [33m850bf85[m wip
[1;34m|[m[1;34m/[m  
* [33m6e1bb3f[m Refactor updateCandlestickData and updateVolumeData to filter out bars with Null status
* [33m7917390[m Add status column to bars table and update related queries
* [33md967313[m Update C++ standard from C++20 to C++23 in README
* [33m8c60065[m Update README.md to reflect current project state
*   [33mf20c060[m Merge pull request #151 from simonbeaudoin0935/copilot/refactor-bar-cache-storage
[1;36m|[m[31m\[m  
[1;36m|[m * [33mf4715cc[m Fix barcache
[1;36m|[m[1;36m/[m  
*   [33m86d98f8[m Merge pull request #153 from simonbeaudoin0935/copilot/optimize-bar-object-memory
[32m|[m[33m\[m  
[32m|[m * [33m121c164[m Add comprehensive documentation for Bar memory optimization
[32m|[m * [33mb299b31[m Update documentation to reflect Bar class changes
[32m|[m * [33ma95cf12[m Optimize Bar class memory footprint
[32m|[m * [33m3b8865c[m Initial plan
[32m|[m[32m/[m  
* [33m0506798[m feat: Add detailed documentation for price chart data loading flow and optimize missing bars request logic
* [33md88e5ec[m feat: Configure splitter for compact display of trade panel in GUIFrontend
* [33m4b7f0d8[m feat: Add replay info label and update functionality in ChartToolbar
* [33m50a7cf4[m feat: Introduce ChartToolbar for improved chart controls
*   [33m684e28e[m Merge pull request #149 from simonbeaudoin0935/copilot/fix-chart-session-background-rectangles
[34m|[m[35m\[m  
[34m|[m * [33md5d83a7[m[33m ([m[1;31morigin/copilot/fix-chart-session-background-rectangles[m[33m)[m Add comment linking hardcoded times to MarketHours definitions
[34m|[m * [33maf3b688[m Update documentation with third bug fix explanation
[34m|[m * [33mae3b36d[m Fix pre-market session to end at 9:30 AM instead of 10:00 AM
[34m|[m * [33mf8cfa1c[m Fix misleading comment about +1 offset
[34m|[m * [33m76c9d24[m Update documentation with second bug fix explanation
[34m|[m * [33me5dfcab[m Fix rectangle drawing logic to prevent full-screen overlays
[34m|[m * [33m8070b65[m Address code review feedback with improved comments and documentation
[34m|[m * [33m84830ed[m Add comprehensive documentation for session background rendering
[34m|[m * [33mb26cafc[m Fix session background rectangle alignment by using NY timezone consistently
[34m|[m * [33me8630f3[m Initial plan
[34m|[m[34m/[m  
*   [33mbba8d27[m Merge pull request #146 from simonbeaudoin0935/copilot/update-markdown-documentation
[36m|[m[1;31m\[m  
[36m|[m * [33md24b400[m[33m ([m[1;31morigin/copilot/update-markdown-documentation[m[33m)[m Add note about StockPriceChart_Architecture.md needing update for qcustomplot
[36m|[m * [33m72622ff[m Update MIGRATION_SUMMARY and Doc README with completion status and current state
[36m|[m * [33ma3c25d6[m Update ProgramSequence, StockPriceChart docs for qcustomplot, mark migration complete
[36m|[m * [33maf1ec8f[m Update Architecture and GUI docs - remove obsolete references, add missing components
[36m|[m[36m/[m  
*   [33m8ef74a1[m Merge pull request #145 from simonbeaudoin0935/copilot/zoom-functionality-for-charts
[1;32m|[m[1;33m\[m  
[1;32m|[m * [33m7f849aa[m[33m ([m[1;31morigin/copilot/zoom-functionality-for-charts[m[33m)[m Keep volume chart Y-axis lower bound at 0 when zooming
[1;32m|[m * [33ma8795c2[m Fix Qt version compatibility for QWheelEvent position method
[1;32m|[m * [33m86fc29e[m Implement differential zoom behavior for price and volume charts
[1;32m|[m[1;32m/[m  
* [33m90e2492[m Center stock symbol input text and adjust chart labels for improved UI
* [33m03d569d[m Implement event filtering for mouse wheel events in StockPriceChart
* [33m4d7434a[m take out closed market background rectancles
* [33m3343c66[m Add volume chart visibility toggle and related functionality
*   [33m5848c50[m Merge pull request #143 from simonbeaudoin0935/copilot/integrate-qcustomplot-library
[1;34m|[m[1;35m\[m  
[1;34m|[m * [33m15ba04c[m[33m ([m[1;31morigin/copilot/integrate-qcustomplot-library[m[33m)[m Fix background rectangles when scrolling beyond available data range
[1;34m|[m * [33mdc7c776[m Fix background rectangles to draw correctly when zoomed to single session
[1;34m|[m * [33m7dc00aa[m Make background colors more visible for testing (higher alpha)
[1;34m|[m * [33mb47972c[m Add debugging output and layer setting for background rectangles
[1;34m|[m * [33m41f1cc0[m Add market session background colors (pre-market, after-hours, closed)
[1;34m|[m * [33mb55ec5c[m Add volume chart below price chart with up/down coloring
[1;34m|[m * [33mf38a736[m Fix layout: Add stretch factors to give chart most of the space
[1;34m|[m * [33m5083c24[m Remove old backup files that were causing build errors
[1;34m|[m * [33m475bd4c[m Add comprehensive migration summary documentation
[1;34m|[m * [33m72fdc39[m Fix header inconsistencies and add migration documentation
[1;34m|[m * [33mf8c2e09[m Integrate qcustomplot library and refactor StockPriceChart implementation
[1;34m|[m * [33m9c332bb[m Initial plan
[1;34m|[m[1;34m/[m  
*   [33m0ac5a18[m Merge pull request #141 from simonbeaudoin0935/fix/price-chart
[1;36m|[m[31m\[m  
[1;36m|[m * [33mfd9e5af[m[33m ([m[1;31morigin/fix/price-chart[m[33m)[m addition of qcustomplot library
[1;36m|[m * [33m2c2d8bf[m wip
[1;36m|[m * [33mb29c21d[m wip
[1;36m|[m * [33mcafbd51[m wip
[1;36m|[m * [33m1e44a21[m wip
[1;36m|[m * [33m63629ed[m wip
[1;36m|[m[1;36m/[m  
* [33m49833a5[m progress
*   [33mc4cd23c[m Merge pull request #140 from simonbeaudoin0935/copilot/adjust-axis-label-density
[32m|[m[33m\[m  
[32m|[m * [33m57a0dbd[m[33m ([m[1;31morigin/copilot/adjust-axis-label-density[m[33m)[m progress
[32m|[m * [33m3ac6cf2[m Refactor logging in StockPriceChart to use simplified macros and improve readability
[32m|[m * [33m240a2fc[m Fix build error: Remove invalid axis->update() calls
[32m|[m * [33m297a0e7[m Add explicit axis update() calls to force tick interval application
[32m|[m * [33ma1f276d[m Add more diagnostic logging before assert to track wheel zoom issue
[32m|[m * [33m35b4334[m Add logging to diagnose tick interval not decreasing when zooming back in
[32m|[m * [33m2abbea2[m Force chart update after setting tick intervals to apply changes immediately
[32m|[m * [33m7ae241f[m Add diagnostic logging to track updateAxisLabels() execution
[32m|[m * [33m81e6d03[m turn if into assert
[32m|[m * [33mdfc71bf[m Fix missing updateAxisLabels() calls in Y-axis zoom and pan operations
[32m|[m * [33m479b20d[m Add detailed logging for tick interval selection and increase density thresholds
[32m|[m * [33me1288f0[m Add comprehensive documentation for dynamic axis labeling system
[32m|[m * [33m63f5117[m Implement dynamic tick interval adjustment for X and Y axes based on screen density
[32m|[m * [33m672aecf[m Initial plan
[32m|[m * [33m261a422[m Optimize missing bars request handling and improve historical bars indexing logic
[32m|[m * [33m4b89699[m Refactor Bar timestamp handling and remove ajustTimeStampToOpeningMinute method
[32m|[m * [33m1b27dd6[m Add debug logging to diagnose historical bar positioning issue
[32m|[m * [33m89e7b3a[m Fix historical bars piling at index 0 by rebuilding series in sorted order
[32m|[m * [33m45c4204[m Improve comments and clarify index mapping logic
[32m|[m * [33m7e8318d[m Fix QCandlestickSet constructor usage and optimize std::distance call
[32m|[m * [33m737add4[m Add comprehensive implementation documentation
[32m|[m * [33ma2089fa[m Address code review feedback for bidirectional index system
[32m|[m * [33m4f1def9[m Fix index constraints and cleanup in bidirectional index system
[32m|[m * [33m6ead572[m Implement bidirectional index system for chart performance
[32m|[m * [33ma980ef3[m Initial plan
[32m|[m[32m/[m  
* [33m9e45aa2[m Enhance StockPriceChart Y-axis with dynamic ticks and improved label count
* [33m521e9cd[m Refactor build configuration and logging in TSClient streams
* [33ma67103d[m Refactor TSClientStreams to improve logging and streamline network request construction
* [33mb2c6d16[m Refactor MarketDepthQuoteReceiver and PositionsReceiver to improve stream handling and error recovery
* [33mba58966[m refactor
* [33m6e323f7[m move Qt6::Gui library linkage to ENABLE_GUI
* [33m264c16a[m Change selection behavior in OrderWindow to disable row selection
* [33m3c01d8b[m Add settings management to OrderEntryWidget for saving and loading user preferences
* [33mf57c635[m Implement account selection feature in OrderEntryWidget and enhance GUIFrontend integration
* [33mfc01693[m Refactor trade action layout in OrderEntryWidget to use a grid layout for improved organization
* [33m488c7c7[m Enhance order display in OrderWindow to show latest orders first and update status item with cancel button emoji
* [33m7947a00[m Add order cancellation feature in OrderWindow with logging support
* [33m7c8acb6[m Refactor limit and stop price parsing in Order constructor to streamline conversion to double
* [33m65ec1fb[m Enhance limit price parsing in Order constructor to support string and double types with debug logging
* [33m883e660[m Enhance OrderWindow to include DateTime in order display and add MarketHours utility for New York time conversion
* [33m6016232[m Align order details in OrderWindow to center for improved readability
* [33mefe7ff7[m Add OrderStatus enum and enhance status handling in Order and OrderWindow
* [33m0c90c53[m Refactor Order class to use std::optional for limit and stop prices, enhance order parsing logic, and improve logging in OrderWindow
* [33m219a9ec[m Enhance order processing and UI: add debugging logs, support nested JSON structures, and include Order ID in the table view
* [33m2cbb14f[m Refactor error handling in stream receivers to use a unified QException approach and improve logging
* [33mfca9bc1[m Refactor OrderEntryWidget to use radio buttons for trade actions and update button styling dynamically
* [33m6b5b6e1[m bbb
*   [33mc67b180[m Merge pull request #138 from simonbeaudoin0935/copilot/add-order-entry-widget
[34m|[m[35m\[m  
[34m|[m * [33mc452e24[m[33m ([m[1;31morigin/copilot/add-order-entry-widget[m[33m)[m Implement position deletion handling in MainAlgo and PositionsReceiver
[34m|[m * [33md19ed3c[m Add positionDeleted signal and slot implementations for GUI and TUI frontends
[34m|[m * [33m3e5a839[m a
[34m|[m * [33md644366[m Use descriptive variable name for connection
[34m|[m * [33m52b53bc[m Fix logging to use standard qInfo/qCritical
[34m|[m * [33m742b68b[m Add OrderEntryWidget for placing orders
[34m|[m * [33m173f546[m Initial plan
[34m|[m[34m/[m  
*   [33mb01a92a[m Merge pull request #137 from simonbeaudoin0935/copilot/add-file-name-input-widget
[36m|[m[1;31m\[m  
[36m|[m * [33md1a3725[m[33m ([m[1;31morigin/copilot/add-file-name-input-widget[m[33m)[m a
[36m|[m * [33mb2a1d57[m Add save/restore functionality for last selected CSV file path
[36m|[m * [33m1ad9b31[m Replace lambda with member function for CSV file path change handler
[36m|[m * [33m1596aae[m Add Qt::UniqueConnection and assertions to signal connections
[36m|[m * [33mdf9fecd[m Add documentation for RecorderTab CSV input widget feature
[36m|[m * [33m755a35f[m Add CSV file input widget to RecorderTab
[36m|[m * [33m42c7f26[m Initial plan
[36m|[m[36m/[m  
* [33m6fc05b7[m disable assert
* [33m2787bcb[m better comment
*   [33m2230af3[m Merge pull request #136 from simonbeaudoin0935/copilot/add-recorder-functionality
[1;32m|[m[1;33m\[m  
[1;32m|[m * [33mb55685e[m[33m ([m[1;31morigin/copilot/add-recorder-functionality[m[33m)[m Wait for TSClient authentication before restoring last displayed stock
[1;32m|[m * [33mf2a6fb5[m Wait for TSClient authentication before allowing recording to start
[1;32m|[m * [33mb5810c1[m Enhance Stream and LiveStreamDB functionality with improved error handling and symbol retrieval
[1;32m|[m * [33m0cf9a9f[m Fix header guard inconsistency and add comments explaining GUI error handling
[1;32m|[m * [33mca0fe54[m Refactor recorder utility functions to eliminate code duplication
[1;32m|[m * [33mad9544f[m Fix platform-specific code and add error handling for directory creation
[1;32m|[m * [33mc954a9b[m Fix include paths for RecorderTab and add Tabs directory to CMakeLists
[1;32m|[m * [33m1b22cc6[m Add RecorderTab integration to main GUI app
[1;32m|[m * [33m8fde085[m Initial plan
[1;32m|[m[1;32m/[m  
* [33mdb1b0cf[m fixes
*   [33me1391da[m Merge pull request #134 from simonbeaudoin0935/copilot/save-last-selected-stock
[1;34m|[m[1;35m\[m  
[1;34m|[m * [33me287707[m[33m ([m[1;31morigin/copilot/save-last-selected-stock[m[33m)[m fixes
[1;34m|[m * [33m2d127ba[m Refactor to extract displayStock method and fix uppercase handling
[1;34m|[m * [33m34d0f96[m Initialize appStateSettings in Recorder and test executables
[1;34m|[m * [33m5dc8b0e[m Add feature to save and restore last displayed stock
[1;34m|[m * [33mbe2b0a1[m Initial plan
[1;34m|[m[1;34m/[m  
*   [33m4a4eadf[m Merge pull request #133 from simonbeaudoin0935/copilot/add-display-widget-for-stream-orders
[1;36m|[m[31m\[m  
[1;36m|[m * [33m57d17c1[m[33m ([m[1;31morigin/copilot/add-display-widget-for-stream-orders[m[33m)[m Fix compilation
[1;36m|[m * [33m0d9e6bf[m Fix code review issues: spelling, bounds checking, null checks, default cases
[1;36m|[m * [33md6e8efb[m Add Order class implementation and OrdersReceiver/OrderWindow widgets
[1;36m|[m * [33m1d65788[m Initial plan
[1;36m|[m[1;36m/[m  
*   [33mec622f9[m Merge pull request #132 from simonbeaudoin0935/refactor/qvariant
[32m|[m[33m\[m  
[32m|[m * [33mae26f06[m[33m ([m[1;31morigin/refactor/qvariant[m[33m)[m fix streams
[32m|[m * [33md565063[m Streams: use QFuture
[32m|[m * [33mbd3c77a[m Refactor Stream class to support template types and enhance error handling with custom exceptions
[32m|[m * [33m03c93dd[m Update C++ standard to C++20 in CMake configuration
[32m|[m * [33mb3fe270[m Enhance Qt6 integration and improve async handling in BarCache
[32m|[m * [33me54a91b[m working
[32m|[m * [33mfefbbab[m checkpoint
[32m|[m * [33meac6cca[m checkpoint barcache
[32m|[m * [33m83229eb[m checkpoint
[32m|[m * [33m353ac69[m checkpoint
[32m|[m[32m/[m  
* [33mf7a0111[m feat: remove FMP data usage handling from FrontEnd and related classes
* [33m71b8bcd[m[33m ([m[1;31morigin/copilot/connect-balance-widget-to-gui[m[33m)[m feat: implement singleton pattern for MainAlgo and adjust balance polling initialization
* [33m3886450[m Add BalanceWindow widget and connect balance updates to GUI
* [33m9bb69f6[m feat: manage stream count and cleanup network replies on destruction
* [33m95c069b[m feat: implement balance polling and related functionality
* [33m6225749[m[33m ([m[1;31morigin/refactor/stream[m[33m)[m refactor: remove unused connections and improve logging messages
* [33m1b85480[m checkpoint
* [33m838b94a[m checkpoint
* [33m405d982[m stream checkpoint
* [33m1257770[m checkpoint
* [33mc2eab04[m checkpoint
* [33m93eba09[m CMakeLists.txt: Split Qt6 modules inclusion based on GUI/TUI
* [33mc361d4c[m Fix debian packaging
* [33mb1c733c[m Deleted doc created by copilot during premium requests
* [33m6cc342e[m Refactor RESTClient and Stream classes for improved error handling and logging; add new signal for reply readiness
* [33m1796395[m Enhance error handling for async requests in MainAlgo and RESTClient; implement retry logic for token refresh in TSClient
*   [33m1462738[m Merge pull request #126 from simonbeaudoin0935/have-async-requests-return-handle
[34m|[m[35m\[m  
[34m|[m * [33m83b20bd[m[33m ([m[1;31morigin/have-async-requests-return-handle[m[33m)[m Refactor RESTClient and TSClient for improved logging and request handling; adjust BarCache range assertion for correctness
[34m|[m * [33m897f338[m Refactor RESTClient and TSClient to use pointers for thread and network manager, ensuring proper memory management and thread safety
[34m|[m * [33m672c03d[m checkpoint
[34m|[m * [33m5b969f5[m checkpoint
[34m|[m * [33m7d8b154[m checkpoint
[34m|[m * [33m39b2cba[m Refactor TSClient and BarCache for improved readability and consistency in method definitions
[34m|[m * [33m455f776[m wip
[34m|[m * [33me1b2a9f[m checkpoint
[34m|[m * [33m5ef23fc[m Refactor RESTClient and TSClient to remove unused jsonDocument pointer and update refreshAsyncAccessToken return type
[34m|[m * [33m58c7b4e[m checkpoint
[34m|[m[34m/[m  
* [33m89c0dff[m Add auto timeframe selection feature to TimeFrameSelector widget
* [33m656ddab[m Revise Copilot instructions for L2Trader repository
*   [33m662a790[m Merge pull request #125 from simonbeaudoin0935/copilot/optimize-stock-price-chart-again
[36m|[m[1;31m\[m  
[36m|[m * [33mfeb1b3c[m[33m ([m[1;31morigin/copilot/optimize-stock-price-chart-again[m[33m)[m Consolidate consecutive session hours into single background rectangles
[36m|[m * [33m4324a2f[m Optimize visible bar price range calculation using binary search instead of full iteration
[36m|[m * [33maedcbf3[m[33m ([m[1;33mtag: [m[1;33myeeee[m[33m)[m Replace debouncing with binary search optimization for real-time background rendering
[36m|[m * [33mff32f43[m Add panning performance comparison documentation
[36m|[m * [33ma6925a4[m Debounce background rendering during pan/zoom to eliminate O(h×d) redundant redraws
[36m|[m * [33m37260e6[m Add performance comparison documentation
[36m|[m * [33m471a706[m Optimize StockPriceChart: Implement incremental updates to eliminate O(n) rebuilds on every bar
[36m|[m * [33mbb63456[m Initial plan
[36m|[m[36m/[m  
*   [33mf1bef05[m Merge pull request #122 from simonbeaudoin0935/copilot/handle-fetching-bars-yesterday
[1;32m|[m[1;33m\[m  
[1;32m|[m * [33m564b43e[m Code polish: fix duplicate public, improve error messages
[1;32m|[m * [33m7437ce0[m Fix: Remove undefined TRADING_END_MINUTE reference
[1;32m|[m * [33m2ba5e90[m Improve code consistency: use MONDAY constant throughout
[1;32m|[m * [33me3a06af[m Address code review feedback: add named constants and helper methods
[1;32m|[m * [33m038f340[m Add support for multi-day bar requests and market-hours-aware timestamp extrapolation
[1;32m|[m[1;32m/[m  
*   [33m4eac4bb[m Merge pull request #115 from simonbeaudoin0935/copilot/fix-stock-price-chart-widget
[1;34m|[m[1;35m\[m  
[1;34m|[m * [33m8f8f502[m[33m ([m[1;31morigin/copilot/fix-stock-price-chart-widget[m[33m)[m fix: Update time zone assertions in getBars and log missing bars requests in StockPriceChart
[1;34m|[m * [33m98fa579[m fix: Adjust time zone handling in getBars and improve error logging in BarCache
[1;34m|[m * [33md912d20[m refactor: Remove synchronous access token refresh method to prevent deadlock
[1;34m|[m * [33m7258a91[m fix: Update axis label handling and adjust tick intervals for better chart readability
[1;34m|[m * [33m1b7369c[m fix: Reduce maximum width of stock symbol input field for better layout
[1;34m|[m * [33m3bf4571[m feat: Add stock symbol validation to ensure proper input format
[1;34m|[m * [33m2a57d3d[m fix: Simplify chart title to display only the stock symbol
[1;34m|[m * [33m96f28f7[m fix: Update include paths for GUI and TUI frontend headers
[1;34m|[m * [33m8754290[m fix: Ensure Y-axis range does not go below zero during panning and zooming
[1;34m|[m * [33me5db9bc[m fix: Hide chart legend and remove axis titles for cleaner visualization
[1;34m|[m * [33m77656d0[m feat: Add StockPriceChart and TimeFrameSelector for enhanced stock data visualization
[1;34m|[m * [33m833bbb1[m Add TimeFrameSelector widget and integrate with StockPriceChart for timeframe selection
[1;34m|[m * [33m345e7f0[m Remove title text from Y-axis in StockPriceChart initialization
[1;34m|[m * [33mfe4fdf9[m Add CacheTab and LoggingTab classes for cache management and logging controls
[1;34m|[m * [33me3a92db[m Add StockPriceChart class with mouse interaction and zoom functionality
[1;34m|[m * [33mbe70b25[m Enhance StockPriceChart with detailed documentation for key methods
[1;34m|[m * [33mccfb5a6[m Add comprehensive implementation summary document
[1;34m|[m * [33mea555b2[m Update StockPriceChart architecture documentation for index-based system
[1;34m|[m * [33ma558c70[m Fix maintainBarLimit to handle both completed and void bars
[1;34m|[m * [33mc8eab6d[m Fix void bar handling to work with index-based positioning
[1;34m|[m * [33m910452c[m Implement index-based positioning system for stock price chart
[1;34m|[m[1;34m/[m  
* [33m1fd95d2[m Update task commands to reflect renamed src folder to Src
* [33m7570d73[m Rename src folder to Src
*   [33m0276458[m Merge pull request #108 from simonbeaudoin0935/copilot/refactor-tui-structure
[1;36m|[m[31m\[m  
[1;36m|[m * [33mb91b376[m Add missing parameter names in function declarations for clarity
[1;36m|[m * [33m40aa307[m Add MainAlgo parameter to TUIFrontend for consistency with GUIFrontend
[1;36m|[m * [33m502f00e[m Fix code review issues - add TUI instantiation, fix typos, remove unused method
[1;36m|[m * [33m27e8a05[m Update documentation to reflect GUIFrontend renaming
[1;36m|[m * [33m14b705e[m Refactor TUI organization - move Terminal to TUI folder and rename classes
[1;36m|[m * [33mc71d278[m Initial plan
[1;36m|[m[1;36m/[m  
* [33mf4806bd[m Enhance live log display: add conditional auto-scroll based on user position
*   [33me7d46a4[m[33m ([m[1;31morigin/copilot/assert-unique-connection[m[33m)[m Merge pull request #104 from simonbeaudoin0935/copilot/investigate-open-stream-count
[32m|[m[33m\[m  
[32m|[m * [33m2d26481[m Replace warning with Q_ASSERT_X for duplicate stream detection
[32m|[m * [33m5641485[m Remove orphaned function declaration from PositionsReceiver
[32m|[m * [33m8f562f6[m Fix duplicate position stream opening issue
[32m|[m[32m/[m  
* [33m56e11b3[m Update Copilot instructions with new guidelines
*   [33mfa1104c[m Merge pull request #100 from simonbeaudoin0935/copilot/add-unit-tests-for-barcache
[34m|[m[35m\[m  
[34m|[m * [33m4ed0a9c[m[33m ([m[1;31morigin/copilot/add-unit-tests-for-barcache[m[33m)[m Add colored message handler reinstallation for QTest and update BarCache tests
[34m|[m * [33m8e06cf1[m Add comprehensive unit tests for BarCache class
[34m|[m[34m/[m  
* [33m89ac024[m Update test command path for barcache test
* [33m6b6f1b1[m Remove CodeQL artifact and add to gitignore
* [33mb800585[m Final update - all Stream signal connections completed
* [33m60ec219[m Address code review feedback - clarify error handling comments
* [33m763692b[m Connect Stream::streamErrorOccurred signal and improve error handling
* [33m6eadd9c[m[33m ([m[1;31morigin/copilot/connect-stream-error-signal[m[33m)[m Update copilot-instructions.md
*   [33m632aeee[m Merge pull request #98 from simonbeaudoin0935/copilot/add-folder-option-recorder-again
[36m|[m[1;31m\[m  
[36m|[m * [33m9dae7fa[m Move Recorder documentation to README and remove TESTING_NOTES.md
[36m|[m * [33m971e887[m Revert .gitignore changes as requested
[36m|[m * [33mb7c184e[m Add _codeql_detected_source_root to .gitignore
[36m|[m * [33ma95e730[m Add testing documentation for --recorded-data-dir option
[36m|[m * [33mdd53fa9[m Add --recorded-data-dir option for Recorder app
[36m|[m * [33mb1cc21a[m Initial plan
* [1;31m|[m   [33m2bee09c[m Merge pull request #95 from simonbeaudoin0935/copilot/add-hello-world-code
[1;31m|[m[1;33m\[m [1;31m\[m  
[1;31m|[m [1;33m|[m[1;31m/[m  
[1;31m|[m[1;31m/[m[1;33m|[m   
[1;31m|[m * [33m2a483fb[m Add comprehensive documentation for WatchApp
[1;31m|[m * [33md896a87[m Create WatchApp: Wear OS Galaxy Watch companion app boilerplate
[1;31m|[m * [33mee3a54d[m Initial plan
[1;31m|[m[1;31m/[m  
*   [33m62efdb9[m Merge pull request #93 from simonbeaudoin0935/copilot/fix-git-version-unknown
[1;34m|[m[1;35m\[m  
[1;34m|[m * [33m1d6580e[m[33m ([m[1;31morigin/copilot/fix-git-version-unknown[m[33m)[m Fix Git version detection by passing env vars to dpkg-buildpackage
[1;34m|[m * [33mda39444[m Fix Git version detection in Debian package builds
[1;34m|[m[1;34m/[m  
*   [33mad46c4d[m Merge pull request #94 from simonbeaudoin0935/feature/add-marketdepth-recording
[1;36m|[m[31m\[m  
[1;36m|[m * [33m2f75a8f[m[33m ([m[1;31morigin/feature/add-marketdepth-recording[m[33m)[m Add automatic stream recovery and recovery statistics tracking to LiveStreamDB and StatusReporter
[1;36m|[m * [33m38ff984[m Add StatusReporter and RecorderUtils for enhanced stream status reporting and error handling
[1;36m|[m * [33mb7e9477[m Refactor database handling by replacing LiveBarsDB and LiveMarketDepthQuoteDB with a unified LiveStreamDB class for improved code maintainability and flexibility.
[1;36m|[m * [33mda5bb86[m Add timeout recovery tracking and statistics to LiveBarsDB and LiveMarketDepthQuoteDB
[1;36m|[m * [33mcff9e45[m Add error handling for stream connections in LiveBarsDB and LiveMarketDepthQuoteDB
[1;36m|[m * [33mbd7dde4[m Add global debug and info message control to logging system
[1;36m|[m * [33m2c22448[m Add writeConfigToDisk method to LoggingConfig for saving category states
[1;36m|[m * [33m034325a[m Add database analysis script for LiveBarsDB and LiveMarketDepthQuoteDB
[1;36m|[m * [33mc66504f[m Add LiveMarketDepthQuoteDB class for market depth data recording
[1;36m|[m * [33m99ea218[m Fix cache location path by using application name for directory
[1;36m|[m * [33m4f2cfb9[m Add mini Nasdaq screener example configuration to documentation
[1;36m|[m * [33m3549ba8[m Remove cache root directory argument from recorder test command
[1;36m|[m * [33m3b29af3[m Add market depth recording functionality and related database setup
[1;36m|[m[1;36m/[m  
*   [33m046c812[m Merge pull request #90 from simonbeaudoin0935/feature/recorder
[32m|[m[33m\[m  
[32m|[m * [33m5866008[m[33m ([m[1;31morigin/feature/recorder[m[33m)[m Clean up Recorder logs before running integration test
[32m|[m * [33mfc04409[m Enhance Recorder integration test to capture and log CRIT and WARN messages from output
[32m|[m * [33me84091d[m Add log display step to Recorder integration test workflow
[32m|[m * [33m0faf9e8[m Add integration test for recorder to check for critical log messages and improve error logging in Stream class
[32m|[m * [33m6e7ac45[m Add Recorder integration test to CI workflow and update installation rules
[32m|[m * [33m1d114c1[m Add Recorder integration test to CMake configuration
[32m|[m * [33m87b810b[m Add error handling for InvalidSymbol and improve logging in Stream class
[32m|[m * [33m3319a1c[m Refactor stream opening methods to use a templated approach for better code reuse and clarity
[32m|[m * [33ma41f977[m Update tasks.json to use mini CSV file and enhance logging in LiveBarsDB
[32m|[m * [33mf0d9a4e[m Update CMake configuration to enable recorder build and fix recorder binary installation path
[32m|[m * [33mdb96271[m Update build tasks in tasks.json and modify TestBarCache to check for Null status instead of Void
[32m|[m * [33me270ae4[m Refactor LiveBarsDB to replace JSON handling with raw data storage and update related methods
[32m|[m * [33mba9a303[m Refactor Stream classes to include symbol in constructor and update signal handling for JSON data
[32m|[m * [33m5efabc2[m Refactor LiveBarsDB and Stream classes to improve signal connections and enhance recording logic
[32m|[m * [33m5d91a2d[m Enhance LiveBarsDB to support JSON recording and update StreamBars for JSON emission
[32m|[m * [33m87cd8b8[m Remove unnecessary entries from .gitignore for build artifacts and Makefile
[32m|[m * [33mce52f6f[m Refactor LiveBarsDB to use SqlQueries for database operations and update storeBarJson method to accept JSON data
[32m|[m * [33m74b1d15[m Add LiveBarsDB class for managing live bars database operations
[32m|[m * [33m6f37cd1[m Refactor initializeBarsDatabase to return LiveBarsDB instance and update main logic to handle it
[32m|[m * [33m568615c[m Refactor recorder structure: move main logic to new files and update CMake configuration
[32m|[m * [33m5e29ba9[m Add function to load stock tickers from CSV file and log the count
[32m|[m * [33m54546c5[m Update recorder build paths and command references for consistency
[32m|[m * [33mc1590f8[m Refactor BarStatus enumeration and update related logic to use 'Null' instead of 'Void'
[32m|[m * [33m02d113f[m Enhance build tasks for recorder integration and improve clean command
[32m|[m[32m/[m  
* [33m40dc49f[m[33m ([m[1;31morigin/ccache-improvements[m[33m)[m Update ccache keys for Debian builds to include architecture and branch information
*   [33m31e5812[m Merge pull request #87 from simonbeaudoin0935/copilot/fix-ccache-upload-retrieval
[34m|[m[35m\[m  
[34m|[m * [33m84c2120[m Set CCACHE_DIR environment variable to fix artifact caching
[34m|[m * [33m27b9038[m[33m ([m[1;31morigin/copilot/fix-ccache-upload-retrieval[m[33m)[m Fix ccache upload by adding save-always flag to cache configuration
[34m|[m * [33m85a8a98[m Initial plan
[34m|[m[34m/[m  
* [33m25ca2d8[m Add 'tree' package to Dockerfiles for amd64 and Raspberry Pi builds
* [33ma6bafdb[m[33m ([m[1;31morigin/copilot/use-ccache-for-builds[m[33m)[m Refactor StockPriceChart documentation to enhance clarity and structure, including comprehensive architecture overview and detailed Mermaid diagrams for various components and interactions.
* [33mf807952[m Update README.md
* [33m23fa0d8[m Fix malformed content at end of copilot-instructions.md
*   [33md84fc73[m Merge pull request #81 from simonbeaudoin0935/copilot/document-stock-price-chart
[36m|[m[1;31m\[m  
[36m|[m * [33md676829[m[33m ([m[1;31morigin/copilot/document-stock-price-chart[m[33m)[m Fix Mermaid syntax error in Bar Limit Management diagram
[36m|[m * [33m60d7534[m Fix Mermaid stateDiagram syntax error in Chart State Variables section
[36m|[m * [33m30826d6[m Update README files to reference new StockPriceChart architecture documentation
[36m|[m * [33mfbf29df[m Add comprehensive StockPriceChart architecture documentation with crash analysis
[36m|[m * [33m18a9a4b[m Initial plan
[36m|[m[36m/[m  
* [33m4e4d1ec[m Implement ccache main branch caching strategy
*   [33mee80fce[m Merge pull request #76 from simonbeaudoin0935/copilot/use-ccache-for-builds
[1;32m|[m[1;33m\[m  
[1;32m|[m * [33mebcc221[m Fix ccache directory path to ~/.cache/ccache
[1;32m|[m * [33m602980d[m Fix cache path validation error by creating ~/.ccache directory
[1;32m|[m * [33mc92fd97[m Add detailed implementation notes answering key considerations
[1;32m|[m * [33mf777654[m Add ccache documentation and implementation notes
[1;32m|[m * [33mf4578d6[m Add ccache support for ephemeral GitHub Actions runners
[1;32m|[m[1;32m/[m  
* [33m247da1e[m Comment out FMPClient unit test in build workflow for future removal
*   [33m7dc342d[m Merge pull request #77 from simonbeaudoin0935/add-ccache-dependency
[1;34m|[m[1;35m\[m  
[1;34m|[m * [33meaf2282[m[33m ([m[1;31morigin/add-ccache-dependency[m[33m)[m Rename build job for amd64 to build-image-amd64-noble for clarity
[1;34m|[m * [33md7151c1[m Add ccache dependency and configure CMake to use ccache for faster builds
[1;34m|[m * [33m4f5dc71[m Maximize main window on startup for improved user experience
[1;34m|[m[1;34m/[m  
*   [33m61db961[m Merge pull request #72 from simonbeaudoin0935/copilot/improve-logger-widget-visibility
[1;36m|[m[31m\[m  
[1;36m|[m * [33maf7cdab[m Add logger widget controls and HTML color support
[1;36m|[m[1;36m/[m  
* [33m13771e6[m Add ccache configuration for faster rebuilds
* [33m2f2b758[m[33m ([m[1;31morigin/copilot/improve-logger-widget[m[33m)[m Fix thread safety issue: Pass nullptr as parent to SecureStorage in FMPClient
* [33m1513192[m Add unit test for stream count functionality
* [33m0d8c56b[m Add stream count tracking and display in status bar
*   [33ma371d06[m Merge pull request #30 from simonbeaudoin0935/copilot/fix-app-crash-after-authentication
[32m|[m[33m\[m  
[32m|[m * [33mc9b5d25[m[33m ([m[1;31morigin/copilot/fix-app-crash-after-authentication[m[33m)[m Fix verified and ready for review
[32m|[m * [33m6b6f6e6[m Remove .conf line and add automatic keyring cleanup
[32m|[m[32m/[m  
* [33m2b65aba[m Remove test-package-installation job from build workflow
* [33mb917dc2[m Add checkout step before downloading Debian package artifacts
* [33m102ba50[m Fix crash after authentication by using stack allocation for SecureStorage
*   [33mac93510[m[33m ([m[1;31morigin/copilot/add-stocks-streams-widget[m[33m)[m Merge pull request #57 from simonbeaudoin0935/copilot/fix-barcache-test-error
[34m|[m[35m\[m  
[34m|[m * [33m3fab806[m[33m ([m[1;31morigin/copilot/fix-barcache-test-error[m[33m)[m Setup test credentials for BarCache unit test execution
[34m|[m * [33m77a25bc[m Fix BarCache unit test command by removing unnecessary CSV argument
[34m|[m * [33madcd9fe[m Update .gitignore to remove unnecessary entries and delete unused CodeQL source root file
[34m|[m[34m/[m  
* [33mc7fef3e[m Update .gitignore to remove unnecessary entries and delete unused CodeQL source root file
* [33m6da50ad[m Allow RPI tests to run on push events in build workflow
* [33ma35fd0f[m Remove redundant build target options and streamline CMake configuration for tests and recorder
* [33ma2d830b[m Redact POST data in fetchAsync method for security
* [33ma7ccab2[m Simplify file installation by directly specifying files
* [33m9061e1f[m Fix CSV file installation in debian package
* [33m57c4d3f[m Update task dependency for run to use build-main-only
* [33m9823f8b[m Refactor build configuration and logging: - Update task labels for clarity in tasks.json - Disable tests and recorder build options by default in CMakeLists.txt - Make recorder executable optional based on BUILD_RECORDER flag - Append '.ansi' to log file names for better format indication
* [33mabe611e[m Remove hardcoded Qt6 t64 dependencies to fix Raspberry Pi installation
* [33meccc0b9[m Add Copilot instructions for building, testing, and validating L2Trader
*   [33md8b7ac4[m Merge pull request #50 from simonbeaudoin0935/copilot/create-auth-config-files
[36m|[m[1;31m\[m  
[36m|[m * [33m9e34770[m[33m ([m[1;31morigin/copilot/create-auth-config-files[m[33m)[m Remove credential setup from build-main dependent test jobs
[36m|[m * [33m31707d9[m Add credential setup to RunUpDetector test jobs
[36m|[m * [33ma821d2c[m Remove double obfuscation - secrets are already obfuscated
[36m|[m * [33m6dc44df[m Add script and workflow steps to create auth config files for tests
[36m|[m * [33mb9f7e9d[m Initial plan
[36m|[m[36m/[m  
*   [33m30e035d[m Merge pull request #48 from simonbeaudoin0935/copilot/fix-rpi-cross-compilation
[1;32m|[m[1;33m\[m  
[1;32m|[m * [33m662d43f[m Fix RPI cross-compilation: remove non-existent toolchain file and use DEB_HOST_GNU_TYPE
[1;32m|[m * [33m6faed00[m Initial plan
[1;32m|[m[1;32m/[m  
*   [33m7f3080c[m Merge pull request #46 from simonbeaudoin0935/copilot/create-docker-container-rpi
[1;34m|[m[1;35m\[m  
[1;34m|[m * [33m9849f9c[m[33m ([m[1;31morigin/copilot/create-docker-container-rpi[m[33m)[m Fix RPI container to use AMD64 base with cross-compilation toolchain
[1;34m|[m * [33m33f3d83[m Add conditional execution for build-main and RPI tests based on PR draft status
[1;34m|[m * [33m3d7eb70[m Replace github.actor with vars.USERNAME repository variable
[1;34m|[m[1;34m/[m  
* [33m7a477ad[m Add RPI build container and workflow jobs
* [33m8d8df56[m Add parallel build option with a specified degree of parallelism in tasks.json
*   [33md3cfb98[m Merge pull request #44 from simonbeaudoin0935/copilot/do-not-start-recorder-service
[1;36m|[m[31m\[m  
[1;36m|[m * [33mf94d750[m Prevent auto-start of Recorder service on package installation
[1;36m|[m[1;36m/[m  
*   [33m1576ca1[m Merge pull request #39 from simonbeaudoin0935/copilot/add-log-box-enable-button
[32m|[m[33m\[m  
[32m|[m * [33mecf723f[m[33m ([m[1;31morigin/copilot/add-log-box-enable-button[m[33m)[m Remove live log display from LoggingTab
[32m|[m * [33md8dba8b[m Refactor build system to use CMake for configuration and parallel builds; remove outdated setup-environment job and streamline test execution.
[32m|[m * [33m190d9a1[m Add moc include for LogBroadcaster QObject
[32m|[m * [33mc578f0d[m Implement log broadcasting and UI reorganization
[32m|[m[32m/[m  
*   [33m0aefde4[m Merge pull request #42 from simonbeaudoin0935/copilot/exclude-tests-from-package-build
[34m|[m[35m\[m  
[34m|[m * [33m059450e[m[33m ([m[1;31morigin/copilot/exclude-tests-from-package-build[m[33m)[m Update container image references to use dynamic GitHub actor
[34m|[m * [33mfdb35ec[m Exclude running tests during debian package build and add separate test jobs
[34m|[m * [33m0e2766b[m Initial plan
[34m|[m[34m/[m  
*   [33mdca6a86[m Merge pull request #37 from simonbeaudoin0935/copilot/switch-to-cmake-build-system
[36m|[m[1;31m\[m  
[36m|[m * [33m101045b[m Exclude FMPClient test from debian package build
[36m|[m * [33m2882b88[m Remove setup-environment dependency from test-package-installation job
[36m|[m * [33m2b29241[m Reorganize src/CMakeLists.txt to group source collection together
[36m|[m * [33ma5b6932[m Remove redundant build-recorder and build-tests jobs
[36m|[m * [33m5c988a2[m Rename MAKE_JOBS to PARALLEL_JOBS for clarity
[36m|[m * [33mc3819e7[m Remove old qmake build files (.pro/.pri/.prf)
[36m|[m * [33m11c7ff5[m Fix .gitignore and remove build directory
[36m|[m * [33m75c391b[m Update build system and documentation for CMake
[36m|[m * [33m5ab17e6[m Add CMake build system support
[36m|[m[36m/[m  
* [33m8262178[m fix: Add cmake to the container
*   [33mb4482c0[m Merge pull request #32 from simonbeaudoin0935/copilot/remove-key-plaintext-print
[1;32m|[m[1;33m\[m  
[1;32m|[m * [33m2dbce83[m Complete security audit - all key/secret prints removed
[1;32m|[m * [33m03fee9a[m Security fix: Remove all plaintext logging of keys, secrets, and tokens
[1;32m|[m[1;32m/[m  
*   [33m410ab36[m Merge pull request #34 from simonbeaudoin0935/copilot/create-oauth-process-markdown
[1;34m|[m[1;35m\[m  
[1;34m|[m * [33m796e357[m Update README to reference new OAuth authentication documentation
[1;34m|[m * [33mad3af2e[m Create comprehensive OAuth authentication documentation with Mermaid diagrams
[1;34m|[m * [33m496c2f2[m Initial plan
[1;34m|[m[1;34m/[m  
* [33m48f16a3[m fix: update build scripts to use qmake6 exclusively for Debian package build
* [33m5a45f50[m fix: update QTimeZone initialization to use constructor directly in test cases
* [33m3d44bb6[m fix: update QTimeZone initialization to use fromName method in test cases
* [33mdf62ddc[m fix: update QTimeZone initialization in test cases and add qmake version check for Debian package build
* [33m386a9d3[m fix: add safe.directory configuration for Debian package build
* [33mc0942a6[m fix: improve build check logic for changes in .github/docker/ folder
*   [33m5fad9f0[m Merge pull request #27 from simonbeaudoin0935/build-noble
[1;36m|[m[31m\[m  
[1;36m|[m * [33m22a0575[m[33m ([m[1;31morigin/build-noble[m[33m)[m fix: simplify Debian package build command by removing unnecessary script sourcing
[1;36m|[m * [33m457047e[m fix: partially revert fcc1c06e Update Debian package build process and remove wrapper script
[1;36m|[m * [33m375860c[m fix: Fix event condition to check for pull_request instead of pull_request_target
* [31m|[m [33m74a055e[m Add git installation to Dockerfile for amd64 build
[31m|[m[31m/[m  
* [33m46b56c2[m Update build workflow to use Ubuntu 24.04 and configure container settings for Debian package build
* [33m26c5077[m Fix condition to prevent pushing to GHCR on pull request events
* [33m0d69010[m Add Dockerfile and workflow for building amd64 container image (#26)
*   [33mf48a2b6[m Merge pull request #25 from simonbeaudoin0935/copilot/add-unit-test-jobs
[32m|[m[33m\[m  
[32m|[m * [33md5ee016[m[33m ([m[1;31morigin/copilot/add-unit-test-jobs[m[33m)[m Use Ubuntu 24.04 for package testing to support Qt6 t64 dependencies
[32m|[m * [33m3c9c392[m Add newline at end of workflow file
[32m|[m * [33m031a435[m Add package installation test job with Docker isolation
[32m|[m * [33m65c1c15[m Initial plan
[32m|[m[32m/[m  
*   [33m27b7627[m Merge pull request #20 from simonbeaudoin0935/copilot/package-app-as-deb
[34m|[m[35m\[m  
[34m|[m * [33mfcc1c06[m[33m ([m[1;31morigin/copilot/package-app-as-deb[m[33m)[m Add wrapper script and update service for Qt library path; adjust package installation paths
[34m|[m * [33m32e6446[m Optimize build process by enabling parallel compilation for main, recorder, and test applications
[34m|[m * [33ma990d3e[m Add override for dh_shlibdeps to handle missing shared library info
[34m|[m * [33m6a11e28[m Remove debian/compat file to fix debhelper compat level conflict
[34m|[m * [33ma52c1c1[m Refactor debian package build to run parallel with other builds
[34m|[m * [33mcb6c6d7[m Add systemd service and timers for Recorder with scheduled daily operation
[34m|[m * [33maf3c354[m Update debian package maintainer to Simon Beaudoin
[34m|[m * [33mb15ede5[m Add workflow permissions for security best practices
[34m|[m * [33m39e9cce[m Add comprehensive debian packaging documentation
[34m|[m * [33m1be1a13[m Update debian package with correct Qt6 dependencies
[34m|[m * [33m34818c4[m Add debian build artifacts to .gitignore
[34m|[m * [33md818bcd[m Add Debian packaging infrastructure with native format
[34m|[m * [33mc0b497a[m Initial plan
[34m|[m[34m/[m  
* [33mcf43b01[m fix: Update build workflow to use laptop Qt path and architecture
* [33mae4b1a9[m fix: Increase parallel jobs for ARM64 architecture in build workflow
* [33mf42c139[m fix: Change stream error logging from fatal to warning in MarketDepthQuoteReceiver
* [33ma6d4ca1[m fix: Improve error handling messages for configuration and file operations
* [33m954d51a[m fix: Refactor build workflow to normalize architecture and qmake command for Raspberry Pi support
* [33mf91a725[m fix: Replace Q_UNREACHABLE_RETURN with Q_UNREACHABLE in Account and Bar classes in order to support older Qt version priod to 6.5
* [33m2096957[m Write comprehensive README with full documentation
* [33md4d9752[m fix: Update test command path and improve token handling in RunUpDetector tests
* [33mfcbe009[m fix: Skip tests outside of regular market hours and adjust time leeway in bar count validation
* [33md4d171e[m fix: Skip tests when market is closed and update test command path
* [33m7b5846c[m fix: Update argument parsing for unit tests and improve criteria file handling
* [33m727ba62[m fix: Update application name and logging initialization in unit tests
* [33m4f7c7d8[m fix: Add Git versioning information to unit test projects and main application
* [33m4327af4[m fix: Remove stock CSV argument from FMPClient, TSClient, and RunUpDetector unit test commands
* [33mc0d9f5d[m fix: Add unit tests for FMPClient, TSClient, and RunUpDetector with stock CSV argument
* [33m384af8b[m fix: Change fatal error to warning for non-existent criteria file in argument parser
* [33m6a8a355[m fix: Refactor build jobs to separate build-main and build-recorder with updated Qt path settings
* [33m92c4e97[m fix: Update run-barcache-test job to use raspberrypi target for ARM64
* [33md54d2d8[m fix: Update build runner target condition for raspberrypi in build-tests job
* [33m3e7e28c[m fix: Correct condition for setting Qt path in build workflow
* [33mf23e5af[m fix: Correct build runner target for raspberrypi in workflow
* [33m8db68b9[m fix: Change default build target from raspberrypi to laptop
* [33m9e946d8[m Add stock CSV file option and integrate into main application flow
* [33m8210754[m fix: Update paths and logging initialization in main and recorder_main
* [33m842422a[m fix: Correct type in onTradeStationAccountsReceived method signature
* [33m5e87b08[m refactor: Remove logging of request headers in fetchAsync and fetchStream methods
* [33m6c4b14e[m feat: Update build workflow to set MAKE_JOBS dynamically based on target
* [33ma325d53[m feat: Refactor Qt path setting in build workflow to use environment variable
* [33m73af17a[m feat: Re-enable Qt installation step in build workflow with conditional execution
* [33m5777114[m feat: Update build workflow to include dynamic target selection for architecture
* [33m12112fa[m feat: Add cache root directory argument to unit tests and update main function logging
*   [33m9ec9f26[m Merge branch 'feature/create-market-recorder': fixing stuff
[36m|[m[1;31m\[m  
[36m|[m * [33mce7557a[m[33m ([m[1;31morigin/feature/create-market-recorder[m[33m)[m refactor: Update include paths for Settings.h to remove unnecessary subdirectory
[36m|[m * [33me84a9a2[m feat: Include Git versioning information in application version string
[36m|[m * [33me2d2976[m feat: Add Recorder module with build tasks and main application logic
[36m|[m * [33m8cd2fca[m feat: Add comprehensive documentation for application architecture, GUI, program sequence, and stock price chart
[36m|[m[36m/[m  
* [33mfe88442[m feat: Refactor cache location handling and update project references
*   [33m4d14657[m Merge pull request #13 from simonbeaudoin0935/copilot/rename-tradingalgorithm-to-l2trader
[1;32m|[m[1;33m\[m  
[1;32m|[m * [33mcce5a61[m Rename TradingAlgorithm.pro to L2Trader.pro and update references
[1;32m|[m * [33mf7eab88[m Initial plan
[1;32m|[m[1;32m/[m  
* [33m7007d03[m feat: Add BarCache unit test job to build workflow
* [33m799798d[m fix: Remove cppcheck installation step from lint workflow
* [33mad860c4[m feat: Add linting and static analysis workflow using cppcheck and clang-tidy
* [33mb253bbe[m fix: Update logging target paths in project files for consistency
* [33m363eff7[m feat: Add dependency between build and build-tests jobs in GitHub Actions workflow
* [33m1c585c7[m fix: Update qmake command paths in build workflow for consistency
* [33m30470f8[m feat: Add GitHub Actions workflow for building and testing the TradingAlgorithm project
* [33m375b127[m Add GitHub Actions workflow to summarize issues
* [33m061562e[m feat: Improve crash handling and stack trace logging with enhanced output to log file
* [33m1c24aa7[m feat: Enhance thread safety in logging by replacing mutex with recursive mutex
* [33meb4e9d2[m feat: Implement thread-safe logging with mutex protection
* [33m6af9e87[m feat: Update project configuration for debug support and enhance crash logging
* [33m915a2a1[m feat: Add comprehensive development log detailing major achievements, system optimizations, and documentation updates
* [33mb5c7c84[m feat: Enhance logging with stack trace support for better error diagnostics
* [33mb58d047[m feat: Add comprehensive application architecture diagram and key interactions documentation
* [33m85cbdf3[m feat: Add StockPriceChart class diagram and key features documentation
* [33m4db3a63[m feat: Add build tasks for testing and update project structure
* [33mc0cc10e[m feat: Add detailed class diagrams and state diagrams for GUI architecture
* [33m8ebeb1c[m Optimize BarCache hierarchy and fix null bar detection
* [33ma27a405[m fix: Enhance logging for cache hits and misses based on API call results
* [33m92cdb7d[m refactor: Optimize BarCache lookup hierarchy and add database assertions
* [33mc961d2a[m feat: Add clearDatabase method to BarCache and update tests to utilize it
* [33m0833c15[m fix: Update logging category handling to correctly handle 'default' category and null context
* [33ma72b9ec[m fix: Adjust logging category handling to exclude 'default' category from output
* [33m2d92818[m feat: Add build and test tasks for BarCache unit tests in VSCode configuration
* [33mc0d2bf4[m refactor: Update flowchart and component diagram in BarCache documentation for clarity and consistency
* [33m9b0d61b[m feat: Add BarCache architecture documentation including class, sequence, and state diagrams
* [33mdb4d0b5[m chore: Remove outdated TODOs related to async request handling and token management
*   [33mf3ecb82[m Merge branch 'feature/secure-storage'
[1;34m|[m[1;35m\[m  
[1;34m|[m * [33m79743c3[m feat: Add assertions to SecureStorage methods for input validation and error handling
[1;34m|[m * [33m1d18c8e[m feat: Enhance SecureStorage with asynchronous operations for storing, retrieving, and deleting values using QKeychain
[1;34m|[m * [33m2ebf4ef[m refactor: Simplify API key retrieval by replacing async operation with synchronous method in FMPClient
[1;34m|[m * [33m3bc52dc[m feat: Implement synchronous methods for secure storage operations to simplify token management
[1;34m|[m * [33mc5e6e9a[m refactor: Change logging level for missing credentials and adjust token loading order
[1;34m|[m * [33m5839477[m feat: Update logging categories for AuthToken and AuthWindow to improve consistency and clarity
[1;34m|[m * [33me6dc1a4[m feat: Integrate SecureStorage for sensitive token management and update settings handling
[1;34m|[m * [33m0ad36b3[m feat: Implement SecureStorage for sensitive data management and update related components
[1;34m|[m[1;34m/[m  
* [33md680be6[m feat: Optimize make command to utilize all available CPU cores
* [33m581106b[m feat: Update logging initialization to use XDG-compliant state directory for logs
*   [33m7a31371[m Merge branch 'feature/add-sqllte' into main
[1;36m|[m[31m\[m  
[1;36m|[m * [33m44cceaf[m Add Cache tab to GUI for cache file management
[1;36m|[m * [33mdc55ad9[m feat: Enhance BarCache database handling with symbol-specific files and logging
[1;36m|[m * [33m67c507c[m feat: Add SQL support to BarCache and Main project files. z
* [31m|[m [33m2a88458[m feat: Rename tabs in GUI to 'Trade' and 'Settings'
[31m|[m[31m/[m  
* [33m1fa0870[m refactor: take theme init  out of gui init function
* [33mf8db34b[m feat: Add shortcut to focus stock symbol input and convert text to uppercase
* [33m0f83646[m feat: Add Ctrl+Q shortcut to quit the application in the GUI
* [33m049b953[m feat: Convert stock symbol input to uppercase and update the input field
* [33m07163ed[m feat: Create logs directory and update log file path for timestamped logs
* [33m2314a0c[m fix: Add missing GUI_ENABLED define to C++ configurations
* [33m4605996[m refactor: Remove unused configSettings and related command-line option from ArgumentParser
*   [33m5279d74[m feat: Add runtime logging category management
[32m|[m[33m\[m  
[32m|[m * [33md5d2eb9[m refactor: Remove logging configuration option and related handling from ArgumentParser
[32m|[m * [33m06f738c[m refactor: Update setCategoryEnabled to write all category states to settings
[32m|[m * [33m4a1967f[m feat: Add LoggingTab for managing logging categories and live log display
[32m|[m * [33m044cf54[m refactor: Separate file and console logging behavior
[32m|[m * [33mbf28ad8[m feat: Add LoggingConfig class for runtime category management
[32m|[m[32m/[m  
* [33m9ef2ecd[m Print the generated logging categories at startup
* [33m6055b7f[m Transfer logging logic in new  files
* [33mabe6d5c[m Add the .vscode folder
* [33m8e88de5[m[33m ([m[1;33mtag: [m[1;33mv1.0[m[33m)[m Adding barCache object name prints
* [33m6a10b30[m refactor: improve logging and debugging infrastructure
* [33me1fe2ad[m added object names for the different Streams object possible to help debugging
* [33m23bad0d[m Improve async error handling and network timeout diagnostics
* [33m128a6ad[m Minor warning and assert improvement
* [33m09179cb[m Added color coding for the log levels
* [33mbb5b99a[m Added configuration files example in  the repo as to have a starting point. Those files are to be supplied with --criterias and --logging
* [33m99f23f3[m RESUMING work after a very long time. Fuck... Im at the beginning of implementing the replay functionality
* [33m586b78f[m feat(StockScreener): add caching and health tech sector
* [33mb6d477c[m fix capital letter bug
* [33ma709205[m refactor(BarCache): improve bar cache warm-up functionality
* [33mc372238[m feat(CompanyScreener): add result serialization
* [33me87d67b[m feat: Implement order streaming and enhance test coverage
* [33m573eb1d[m refactor: Restructure order types and enhance order handling
* [33m7333b77[m feat: Implement async order cancellation and enhance order handling
* [33m64b9942[m feat: Implement order execution and enhance test coverage
* [33m463ccb5[m feat: Implement order streaming and enhance brokerage functionality
* [33m74c1888[m refactor: Implement AccountType enum and update related classes
* [33m0d5eb81[m wip on adding new API calls
* [33m6beb75a[m wip for the void bars added
* [33madd6599[m feat(chart): Improve bar handling and time precision
* [33m85258e8[m wip
* [33m72dc609[m feat: Implement stock symbol selection and GUI improvements
* [33m6c61d72[m refactor: Rename StockRunUpDetector to RunUpDetector
* [33mc908e0a[m feat: Implement stock run-up detection algorithm
* [33m051cc7b[m refactor: Convert Bar class to use proper types and enums
* [33m983b8ea[m Refactor BarCache and StockRunUpDetector for improved after-hours handling
* [33mc85ad1f[m fix: Improve BarCache partial hit handling and restore critical timestamp adjustment
* [33md3f0b0f[m Fixed the cache to pass all the current tests
* [33mc0b97a8[m feat: Implement intelligent partial cache hit handling
* [33m20af155[m feat: Add timestamp adjustment functionality to Bar class
* [33m16a6fb6[m feat: Implement 24-hour market data support and caching system
* [33mf05bd8a[m refactor: improve code organization and error handling
* [33mcebdfb6[m refactor: reorganize project structure and improve include handling
* [33m9e3533f[m feat: Add BarCache implementation and improve test infrastructure
* [33m11b9ab7[m fix: Improve error handling and logging configuration
* [33m56f7845[m feat: Implement async bars retrieval and improve REST client
* [33m9e4088a[m refactor: Rename order placement methods for consistency
* [33mdd1ead3[m refactor: Improve code organization and clean up warnings
* [33m6c786c6[m refactor: Improve configuration and token management
* [33m1959cba[m refactor: Remove unused stream processing methods
* [33mbc2c628[m refactor: Reorganize TSClient methods and improve code structure
* [33mb53ef00[m refactor: Reorganize project structure and market data components
* [33mc1c5a6d[m wip : adding a stock run up detector
* [33m093a393[m Attempt to fix the data dechunk
* [33mefd303e[m feat: Implement QuoteSnapshot class and TSClient integration
* [33m723030a[m refactor: Improve GUI components and market depth calculations
* [33mda43d24[m feat: Add Depth-Weighted Price (DWP) display to market depth table
* [33ma00a283[m fix QGraphicsScene::addItem: item has already been added to this scene warning
* [33m42db821[m y
* [33m43d61b3[m refactor: move bid-ask imbalance from table to gauge widget
* [33m19b3f84[m yyy
* [33m1fb140e[m add two new metrics in the market depth receiver
* [33mf38547a[m wip
* [33m84cab9e[m set candles black
* [33mf842dcd[m removed unused line serie
* [33mc5cfbab[m implement dark theme all over
* [33m85f02e6[m implement right clic
* [33m4b6a804[m fix: preserve chart view state and price label position
* [33md40b3d6[m refactor: use MarketHours utility for session detection in StockPriceChart
* [33m74edab0[m refactor(chart): Improve StockPriceChart code organization and readability
* [33m1bce81b[m feat(gui): Add position window widget for displaying active positions
* [33mfe94474[m feat(positions): Add position update tracking and improve stream status handling
* [33mee752be[m feat(positions): Add positions streaming and monitoring system
* [33m86edb58[m renamed AccoutsResult to Account
* [33m83601e7[m fix zoom spaning accross multiple days
* [33mde05482[m feat(MarketHours): Add MarketHours utility class and integrate with StockPriceChart
* [33m99537e5[m feat(GUI): Enhance MarketDepthTable visualization and layout
* [33m445dcfc[m feat(Algo): Implement MarketDepthQuoteReceiver and integrate with MainAlgo
* [33mdd8d6fd[m fix(GUI): Fix MarketDepthTable sizing and scrolling issues
* [33m808f04b[m feat(GUI): Add MarketDepthTable widget
* [33mdb84278[m WORKING receiving market depth quotes!!!
* [33m92bddf2[m use pragma once create Position class
* [33m3c654d5[m feat(StockPriceChart): Enhance chart navigation and zoom controls
* [33m3331397[m feat(StockPriceChart): Enhance chart with candlesticks and interactive features
* [33m2fb7edb[m refactor: Split MainAlgo into modular components and improve code organization
* [33mcd65d9e[m feat: Implement StreamBars functionality and refactor streaming infrastructure
* [33m2b404a8[m Fixing the   Q_ASSERT(replyToDelete != nullptr);
* [33m937677b[m Fixed nullptr dereference causing Stream::onFinished to crash
* [33m798ae18[m Somewhat major refactor to properly integrate the HTTP STREAM capability. In the prior commit it was close to working, but then realized that the slot finished() for the networkmanager AND created streams would get called for the same shit Has to refactor the crap out of everything, so that all the network shit remains in the RESTClient.
* [33ma19b986[m feat: Implement Market Depth Quote HTTP Streaming
* [33m05b1af3[m refactor: Improve PlaceOrderResult handling and code quality
* [33m5edc69b[m feat(TSClient): Add account validation and improve test infrastructure
* [33m8328aac[m refactor(TradeStation): Rename TradeStationClient to TSClient for brevity
* [33m6c01a67[m wip
* [33m7a7812a[m feat(news): Implement news monitoring and alerting system
* [33ma7e4862[m feat(PlaceOrder): Implement order placement functionality and validation
* [33m13ca9b1[m feat(PlaceOrder): Add PlaceOrder functionality to TradeStationClient
* [33m50158da[m feat(PlaceOrder): Implement TradeStation PlaceOrder API integration
* [33m46a6fe8[m Reorganize the project to have the main.cpp/.pro at the root of the project folder This is so that we dont have to use ../ everywhere in the main.pro
* [33md94a57a[m refactor: Standardize file naming to match class names
* [33m760db41[m refactor: Improve thread management and TradeStation client initialization
* [33m4a945e0[m refactor: Improve TradeStation authentication and token management
* [33m7520187[m refactor: Extract client credentials into ClientToken class
* [33me005c0f[m refactor: Implement AuthToken class and improve token management
* [33m6d641f9[m refactor(api): improve REST client and filter implementations
* [33mc80aa29[m refactor(RESTClient): Add API_KEY_PLACEMENT constant to client classes
* [33m3a9f261[m refactor: rename URL parameter functions to toUrlQuery
* [33m5926bcd[m fix: Update AccountResult to match TradeStation API format
* [33m82cc05a[m ** getAccounts still buggy, need to handle how to parse the object appropriatly
* [33m816fce0[m fetch account ALMOST working, need rework
* [33m537e9ad[m feat: Add TradeStation accounts API integration
* [33m290e613[m refactor: Move FMP filter files to dedicated Filters directory
* [33m2f556d3[m feat: Add header-based authentication support for TradeStation API
* [33m4b50916[m test: Improve TradeStation authentication tests
* [33m4bc9b51[m turn off the test fetchquote and algo for now
* [33mb1fdbbd[m refactor: Improve FMP data usage signal propagation
* [33m8c5a326[m refactor: Improve TradeStation authentication signal propagation
* [33m4778e42[m refactor(AuthWindow): Improve token management and validation
* [33m83a0af1[m feat: Add TradeStation login button to status bar
* [33m57c3b26[m things are compiling main and test
* [33m252bda6[m default tab is tab 0,added auth dialog sources
* [33mcfe82f8[m feat(auth): Implement TradeStation OAuth2 authentication window
* [33mcee089b[m refactor: Convert API key handling from static to instance member
* [33m2d9ade0[m refactor: Refactor TradeStationClient to use RESTClient base class
* [33m99bd431[m feractor : Taken out AppInterface unused interface function
* [33m6d45a6a[m refactor: Extract common REST client functionality into base class
* [33mbe2a1d6[m refactor: reorganize client directories into Clients folder
* [33medc1a74[m feat: Add TradeStationClient and reorganize test structure
* [33mc724e03[m test: Improve signal handling in FMPClient tests
* [33m015288b[m fixed typed for fetch float. added a memory monitor
* [33m0158e9b[m Added test for max per minute. Added company-screener fetcher and test
* [33m5ffccab[m Fixed quote to short-quote. Piped quote to UI
* [33m0de2ad3[m added metric to mesure the data used
* [33m9ce5f48[m Incorporated the core skeleton of the previous app, like gui, appfrontend, the minimal gui. Reworked the arg parsing
* [33m18c64d5[m working async plus latency test
* [33m16d8561[m Working fake network latency tests
* [33m94d9801[m Added a 6s latency test, plus reorganized FMPClient so that its not a static lib anymore for simplicity
* [33md64ace3[m Begin of a refactor in order to have fetchers for multiple endpoints and avoid duplicating the fetch logic. The sync version is working, but the async not yet. Made fmpclient a singleton. made TestFMPClient class a friend of FMPClient
* [33m355b03e[m Adding an initial working FMPClient that can fetch a quote in a sync and async way. Has a simple qtest suite and simple main.
* [33m16f2925[m Initial commit
