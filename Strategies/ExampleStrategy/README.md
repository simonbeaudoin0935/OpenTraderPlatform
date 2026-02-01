# ExampleStrategy - L2Trader Example Plugin

This is a minimal example trading strategy plugin for L2Trader.

## Building

### Prerequisites
- Qt 6.4.2+
- CMake 3.16+
- C++23 compiler (GCC 7+ or Clang 5+)

### Build Steps

```bash
cd Strategies/ExampleStrategy
mkdir build
cd build
cmake -S .. -B . -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

The compiled plugin will be at: `build/lib/ExampleStrategy.so`

### Installation

Copy the .so file to the strategies directory:
```bash
mkdir -p ~/.local/share/L2Trader/strategies/
cp build/lib/ExampleStrategy.so ~/.local/share/L2Trader/strategies/
```

### Configuration

Copy the config file to the config directory:
```bash
mkdir -p ~/.config/L2Trader/strategies/
cp example_strategy.json ~/.config/L2Trader/strategies/
```

Then edit `~/.config/L2Trader/strategies/example_strategy.json` to adjust the settings:
- `symbols`: List of stock symbols to monitor
- `positionSize`: Shares per trade
- `riskLimit`: Maximum loss per trade
- `customParams`: Custom strategy parameters (as JSON object)

## Running in L2Trader

1. Start L2Trader
2. Go to the **Strategies** tab
3. Click "Load Strategy"
4. Select "Example Strategy" from the registry
5. Click "Load" to start the strategy
6. Monitor the strategy logs to see it processing bars and updates

## What This Strategy Does

- Logs incoming bar data (every 10 bars to avoid spam)
- Logs market depth quotes
- Logs order updates
- Logs position updates
- Logs balance changes
- **Does NOT execute trades** - purely observational for testing

## Customizing

To create your own strategy:

1. Copy this directory to a new name: `MyStrategy`
2. Rename the class and files
3. Implement your trading logic in the callbacks:
   - `onBar()` - Called when a new candle closes
   - `onMarketDepth()` - Called on bid/ask updates
   - `onOrderUpdated()` - Called when your orders change
   - `onPositionUpdated()` - Called when positions change
   - `onBalanceUpdated()` - Called when account balance changes
4. Use `getSdk()->placeOrder()` to execute trades
5. Use `log()` to record events to the strategy logs
6. Update CMakeLists.txt with your strategy name
7. Build and load!

## API Reference

### StrategyBase Methods

```cpp
// Lifecycle
void onStart(StrategySDK* p_sdk);    // Called when strategy starts
void onStop();                       // Called when strategy stops
void onPause();                      // Called when strategy pauses

// Data callbacks
void onBar(const Bar& p_bar);
void onMarketDepth(const MarketDepthQuote& p_quote);
void onOrderUpdated(const Order& p_order);
void onPositionUpdated(const Position& p_position);
void onBalanceUpdated(double p_balance);
```

### StrategySDK Methods

```cpp
// Place orders
QFuture<std::expected<PlaceOrderResult, TSClient::Error>> 
placeOrder(const PlaceOrderRequest& p_order);

// Access config
StrategyConfig getConfig();

// Logging
StrategyLogger* getLogger();

// Get market data
// (Available through callbacks or direct API calls)
```

### Helper Methods

```cpp
// Logging (thread-safe)
log() << "Message" << value;

// Configuration access
getSdk()->getConfig();
```

## Troubleshooting

**Plugin won't load:**
- Check `~/.local/share/L2Trader/logs/` for error messages
- Verify Qt6 libraries are installed: `ldd ExampleStrategy.so`
- Check config file path is correct in JSON

**Strategy not receiving data:**
- Verify symbols in config match available market data
- Check logs tab to see if errors occurred
- Make sure L2Trader main algorithm is running

**Performance issues:**
- Reduce logging frequency (logging has overhead)
- Check CPU/memory usage in strategy tile
- Profile with `perf` or `valgrind`

## License

See the main L2Trader repository for license information.
