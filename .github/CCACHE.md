# ccache Configuration for CI Builds

This document explains how ccache is configured and used in the L2Trader CI pipeline to speed up builds on GitHub's ephemeral runners.

## Overview

ccache is a compiler cache that stores the results of compilation and reuses them when the same code is compiled again. This significantly speeds up rebuild times, which is especially valuable in CI/CD environments where the same code is often built multiple times (e.g., after small changes).

## How It Works

1. **CMake Integration**: The root `CMakeLists.txt` automatically detects and enables ccache if available (lines 14-19)
2. **Docker Images**: ccache is installed in both build container images:
   - `Dockerfile.amd64.noble` - for X86_64 builds
   - `Dockerfile.rpi.bookworm` - for ARM64 cross-compilation builds
3. **GitHub Actions Cache**: The `~/.ccache` directory is cached between workflow runs using `actions/cache@v4`

## Configuration

### ccache Settings

The following ccache settings are applied in each build job:

```bash
ccache --set-config=max_size=500M          # Limit cache to 500MB per job
ccache --set-config=compression=true       # Enable compression
ccache --set-config=compression_level=6    # Good balance of speed/size
```

### Cache Keys

Different cache keys are used for each build type to prevent cross-contamination:

- **X86_64 builds**: `ccache-debian-noble-${COMMIT_SHA}`
  - Restore fallback: `ccache-debian-noble-`
- **ARM64 builds**: `ccache-debian-bookworm-arm64-${COMMIT_SHA}`
  - Restore fallback: `ccache-debian-bookworm-arm64-`

The cache key includes:
- OS/distribution (noble/bookworm)
- Architecture (implicit for noble, explicit for arm64)
- Commit SHA for the exact key

The restore fallback allows using cache from previous commits when an exact match isn't found.

## Benefits

### First Build (Cache Miss)
- Compilation happens normally
- Results are stored in `~/.ccache`
- Cache is uploaded at job completion
- Build time: ~2-5 minutes (normal)

### Subsequent Builds (Cache Hit)
- Cache is restored before build starts
- Most files compile instantly from cache
- Only changed files are recompiled
- Expected speedup: 50-80% faster builds

### Example Cache Hit Rates
- Minor code changes: 90-95% cache hit rate
- Header file changes: 60-80% cache hit rate (files that include the header must recompile)
- Build system changes: 40-60% cache hit rate

## Monitoring

After each build, ccache statistics are displayed:

```bash
ccache --show-stats
```

This shows:
- Cache hits vs misses
- Cache size and utilization
- Compression ratio

Look for these metrics to understand cache effectiveness.

## Considerations

### Cache Size Limits

- **Per-job limit**: 500M (configured in workflow)
- **GitHub Actions limit**: 10GB total per repository
- **Automatic eviction**: GitHub removes least-recently-used caches when limit is reached
- **Recommendation**: Current settings should keep total cache usage well under 5GB

### Cache Invalidation

Caches are automatically invalidated when:
- Cache key doesn't match (different commit SHA)
- Compiler version changes (new Docker image)
- CMake configuration changes significantly

### Best Practices

1. **Don't increase max_size unnecessarily**: Larger caches take longer to compress/decompress and upload/download
2. **Monitor cache hit rates**: If consistently low (<50%), investigate why
3. **Separate caches by architecture**: Prevents cache corruption from different target platforms
4. **Use compression**: Reduces cache storage and transfer time

### Troubleshooting

If builds aren't getting faster:

1. Check ccache statistics in build logs (`ccache --show-stats`)
2. Verify cache is being restored (look for "Cache restored" in Actions log)
3. Check if compiler/dependencies changed (forces cache miss)
4. Ensure ccache is in PATH and being used by CMake

### Limitations

- **Ephemeral runners only**: Self-hosted runners already have persistent storage
- **First build always slow**: No way around cold cache on first run
- **Not a silver bullet**: Major changes still require full recompilation

## Files Modified

- `.github/docker/Dockerfile.amd64.noble` - Added ccache package
- `.github/docker/Dockerfile.rpi.bookworm` - Added ccache package  
- `.github/workflows/build.yml` - Added ccache configuration and caching steps
- `CMakeLists.txt` - Already had ccache detection (no changes needed)

## Future Improvements

Potential enhancements:
- Fine-tune cache size based on actual usage patterns
- Experiment with different compression levels
- Add cache metrics to build summaries
- Consider ccache for self-hosted runners if storage becomes an issue

## References

- [ccache documentation](https://ccache.dev/)
- [GitHub Actions cache documentation](https://docs.github.com/en/actions/using-workflows/caching-dependencies-to-speed-up-workflows)
- [CMake ccache integration](https://cmake.org/cmake/help/latest/variable/CMAKE_LANG_COMPILER_LAUNCHER.html)
