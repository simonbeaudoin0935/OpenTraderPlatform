# Contributing to L2Trader

Thank you for your interest in contributing to L2Trader! This guide will help you get started.

## Table of Contents

1. [Getting Started](#getting-started)
2. [Development Workflow](#development-workflow)
3. [Code Standards](#code-standards)
4. [Testing Requirements](#testing-requirements)
5. [Pull Request Process](#pull-request-process)
6. [Documentation](#documentation)

## Getting Started

### Prerequisites

Before contributing, ensure you have:

1. **Development environment** set up (see [DEVELOPMENT.md](DEVELOPMENT.md))
2. **Git hooks installed**: `./Utils/install-git-hooks.sh`
3. **Familiarity with**:
   - Qt6 framework
   - C++23 standards
   - Git workflow

### Fork and Clone

```bash
# Fork the repository on GitHub
# Clone your fork
git clone https://github.com/YOUR_USERNAME/L2Trader.git
cd L2Trader

# Add upstream remote
git remote add upstream https://github.com/simonbeaudoin0935/L2Trader.git

# Install git hooks
./Utils/install-git-hooks.sh
```

## Development Workflow

### 1. Create a Feature Branch

```bash
# Update main branch
git checkout main
git pull upstream main

# Create feature branch
git checkout -b feature/amazing-feature
```

**Branch Naming**:
- `feature/description` - New features
- `fix/description` - Bug fixes
- `docs/description` - Documentation updates
- `refactor/description` - Code refactoring
- `test/description` - Test additions/fixes

### 2. Make Changes

Follow the [coding guidelines](#code-standards) and make your changes:

```bash
# Make changes to files
# Build and test
cmake --build build --parallel
ctest --test-dir build --output-on-failure

# Format code (or let pre-commit hook do it)
find Src -name "*.cpp" -o -name "*.h" | xargs clang-format -i
```

### 3. Commit Changes

```bash
# Stage changes
git add .

# Commit with descriptive message
git commit -m "Add amazing feature

Detailed description of what changed and why.
Closes #123"
```

**Commit Message Format**:
```
<type>: <short summary>

<detailed description>

<footer>
```

**Types**:
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation changes
- `style`: Code style/formatting
- `refactor`: Code refactoring
- `test`: Test additions/changes
- `chore`: Build/tooling changes

**Example**:
```
feat: Add bidirectional index system to chart

Implement negative indices for historical bars to avoid
full index rebuild on pan operations. Reduces complexity
from O(n) to O(m) for inserting m historical bars.

Closes #456
```

### 4. Push and Create PR

```bash
# Push to your fork
git push origin feature/amazing-feature

# Create pull request on GitHub
# Use the PR template and fill in all sections
```

## Code Standards

### Code Formatting

The project uses **uncrustify** for automatic formatting:

```bash
# Format specific file
uncrustify -c .uncrustify.cfg --no-backup --replace <file>

# Format all files
find Src -name "*.cpp" -o -name "*.h" | xargs uncrustify -c .uncrustify.cfg --no-backup --replace
```

**Pre-commit Hook**: Automatically checks formatting before each commit.

### Code Style

```cpp
// Member variables: m_ prefix
class MyClass {
private:
    int m_count;
    QString m_name;
};

// Global variables: g_ prefix
int g_instanceCount;

// Parameters: p_ prefix
void function(int p_value);

// Local variables: camelCase
int localValue = 0;

// Constants: UPPER_SNAKE_CASE in CONSTANTS.h
namespace MyConstants {
    constexpr int MAX_SIZE = 100;
}

// Functions: camelCase
void processData();

// Classes: PascalCase
class StockPriceChart;
```

### Memory Management

```cpp
// PREFER: Composition (direct members)
class MyClass {
    BarCache m_cache;  // ✅ Direct member
};

// USE: Qt parent-child for QObjects
QTimer* timer = new QTimer(this);  // ✅ Qt manages

// USE: Smart pointers when necessary
std::unique_ptr<Data> m_data;      // ✅ Exclusive ownership
std::shared_ptr<QVector<Bar>> bars; // ✅ Shared ownership
QPointer<Stream> m_stream;         // ✅ Uncertain lifetime

// AVOID: Raw pointers without management
Data* m_data = new Data();         // ❌ Memory leak risk
```

### Constants and SQL

**Constants** → `Src/Misc/CONSTANTS.h`:
```cpp
namespace MyConstants {
    inline const QString API_URL = "https://api.example.com";
    constexpr int TIMEOUT_MS = 5000;
}
```

**SQL Queries** → `Src/SQL/*Queries.h`:
```cpp
namespace MyDatabaseQueries {
    inline const QString CREATE_TABLE = R"(
        CREATE TABLE IF NOT EXISTS my_table (
            id INTEGER PRIMARY KEY
        )
    )";
}
```

### Assertions and Error Handling

```cpp
// Use ASSUME macros (Src/Misc/Assume.h)
ASSUME(ptr != nullptr);
ASSUME(index >= 0 && index < size);

// Always check pointer allocations
Data* data = new Data();
Q_CHECK_PTR(data);

// Use [[nodiscard]] for important return values
[[nodiscard]] bool saveData();

// Prefer std::expected for operations that can fail
std::expected<Data, Error> loadData();

// Early return for error cases
if (!file.open(QIODevice::ReadOnly)) {
    qCritical() << "Failed to open file";
    return false;
}
// Happy path continues
```

### Threading

```cpp
// PREFER: Stack-allocated threads
class MyClass {
    QThread m_thread;  // ✅ Member variable
    
    ~MyClass() {
        m_thread.quit();
        if (!m_thread.wait(5000)) {
            m_thread.terminate();
            m_thread.wait();
        }
    }
};

// Always use Qt::UniqueConnection
bool connected = connect(sender, &Sender::signal,
                         receiver, &Receiver::slot,
                         Qt::UniqueConnection);
ASSUME(connected);
```

## Testing Requirements

### Unit Tests

All new features must include unit tests:

```cpp
// Tests/<component>_test.cpp
class TestMyFeature : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();      // Setup once
    void testBasicFunctionality();
    void testEdgeCases();
    void testErrorHandling();
    void cleanupTestCase();   // Cleanup once
};

QTEST_MAIN(TestMyFeature)
#include "test_myfeature.moc"
```

### Running Tests

```bash
# Build tests
cmake -S . -B build -DBUILD_TESTS=ON
cmake --build build --parallel

# Run all tests
ctest --test-dir build --output-on-failure

# Run specific test
./build/Tests/test_myfeature
```

### Test Coverage

Aim for:
- **New code**: 80%+ coverage
- **Bug fixes**: Add test that reproduces bug
- **Refactoring**: Maintain existing coverage

### Manual Testing

For UI changes:
1. Test both GUI and TUI modes (if applicable)
2. Test with different window sizes
3. Test with mock data
4. Take screenshots for PR

## Pull Request Process

### Before Submitting

**Checklist**:
- [ ] Code follows style guidelines
- [ ] All tests pass
- [ ] New tests added for new features
- [ ] Documentation updated
- [ ] Commit messages are clear
- [ ] No merge conflicts
- [ ] Pre-commit hook passes

```bash
# Run full validation
cmake --build build --parallel
ctest --test-dir build --output-on-failure
find Src -name "*.cpp" -o -name "*.h" | xargs uncrustify -c .uncrustify.cfg --check
```

### PR Template

Fill out the pull request template completely:

```markdown
## Description
Brief description of changes

## Type of Change
- [ ] Bug fix
- [ ] New feature
- [ ] Breaking change
- [ ] Documentation update

## Testing
Describe testing performed

## Screenshots
If UI changes, include screenshots

## Checklist
- [ ] Code follows style guidelines
- [ ] Tests added/updated
- [ ] Documentation updated
- [ ] All tests pass
```

### Review Process

1. **Automated Checks**: CI runs on all PRs
   - Build verification (x86_64, ARM64)
   - Code formatting check
   - Unit tests
   - UBSan build

2. **Code Review**: Maintainers review code
   - Code quality
   - Architecture fit
   - Test coverage
   - Documentation

3. **Feedback**: Address reviewer comments
   - Make requested changes
   - Push updates to same branch
   - Reply to comments

4. **Approval**: Once approved:
   - Maintainer merges PR
   - Branch deleted automatically

### After Merge

```bash
# Update local main branch
git checkout main
git pull upstream main

# Delete feature branch
git branch -d feature/amazing-feature
git push origin --delete feature/amazing-feature
```

## Documentation

### Code Documentation

```cpp
/**
 * @brief Short description
 * @param p_param Parameter description
 * @return Return value description
 * 
 * Detailed description of what the function does,
 * any side effects, threading requirements, etc.
 */
void myFunction(int p_param);
```

### Markdown Documentation

When adding/updating documentation in `Doc/`:

1. **Use clear structure**:
   ```markdown
   # Title
   
   ## Table of Contents
   
   1. [Section](#section)
   
   ## Section
   
   Content...
   ```

2. **Include diagrams** (Mermaid):
   ```markdown
   ```mermaid
   graph TD
       A[Start] --> B[Process]
       B --> C[End]
   ```
   ```

3. **Add to index**: Update `Doc/README.md`

4. **Cross-reference**: Link related docs

### Updating README.md

If changes affect user-facing features:
- Update main `README.md`
- Update feature list
- Update build instructions if needed
- Add to changelog section

## Getting Help

### Communication Channels

- **Issues**: https://github.com/simonbeaudoin0935/L2Trader/issues
- **Discussions**: https://github.com/simonbeaudoin0935/L2Trader/discussions
- **Email**: Check repository for contact info

### Questions

Before asking:
1. Check existing documentation
2. Search closed issues
3. Review pull requests

When asking:
- Provide context
- Include error messages
- Share minimal reproducible example
- Mention your environment (OS, Qt version, etc.)

## Code of Conduct

### Our Pledge

We pledge to make participation in this project a harassment-free experience for everyone.

### Our Standards

**Positive behavior**:
- Using welcoming language
- Being respectful of differing viewpoints
- Gracefully accepting constructive criticism
- Focusing on what is best for the community

**Unacceptable behavior**:
- Trolling, insulting/derogatory comments
- Public or private harassment
- Publishing others' private information
- Other unethical or unprofessional conduct

### Enforcement

Violations may result in:
1. Warning
2. Temporary ban
3. Permanent ban

## License

By contributing, you agree that your contributions will be licensed under the MIT License.

## Recognition

Contributors are recognized in:
- Git commit history
- Release notes
- Future CONTRIBUTORS.md file

Thank you for contributing to L2Trader!
