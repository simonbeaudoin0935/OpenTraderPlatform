#pragma once

#include <QString>
#include <QMetaType>
#include <cstring>

/**
 * @brief Lightweight ticker symbol class optimized for stock/index symbols
 * 
 * This class replaces QString for stock ticker symbols, providing:
 * - Fixed storage for up to 5 characters plus null terminator
 * - Support for symbols with dots (e.g., BRK.A) and ^ prefix (e.g., ^DJI)
 * - Efficient memory usage (6 bytes vs QString overhead)
 * - Full Qt container compatibility (QMap, QVector, QHash)
 * - Qt signal/slot compatibility via Q_DECLARE_METATYPE
 */
class Ticker {
public:
    // Default constructor - creates empty ticker
    Ticker();
    
    // Constructor from C string
    explicit Ticker(const char* symbol);
    
    // Constructor from QString
    explicit Ticker(const QString& symbol);
    
    // Copy constructor
    Ticker(const Ticker& other);
    
    // Assignment operator
    Ticker& operator=(const Ticker& other);
    
    // Assignment from QString
    Ticker& operator=(const QString& symbol);
    
    // Conversion to QString
    QString toString() const;
    operator QString() const;
    
    // Conversion to C string
    const char* c_str() const;
    
    // Comparison operators for Qt containers
    bool operator==(const Ticker& other) const;
    bool operator!=(const Ticker& other) const;
    bool operator<(const Ticker& other) const;
    bool operator<=(const Ticker& other) const;
    bool operator>(const Ticker& other) const;
    bool operator>=(const Ticker& other) const;
    
    // Comparison with QString
    bool operator==(const QString& other) const;
    bool operator!=(const QString& other) const;
    
    // Check if ticker is empty
    bool isEmpty() const;
    
    // Get length of ticker
    int length() const;
    
    // Validation
    bool isValid() const;
    
    // Hash function for QHash support
    friend uint qHash(const Ticker& ticker, uint seed = 0);
    
private:
    char data[6];  // 5 characters + null terminator
    
    void setSymbol(const char* symbol);
};

Q_DECLARE_METATYPE(Ticker)

// Hash function for QHash
inline uint qHash(const Ticker& ticker, uint seed) {
    return qHash(ticker.c_str(), seed);
}
