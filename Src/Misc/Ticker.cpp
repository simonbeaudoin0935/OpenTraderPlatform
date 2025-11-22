#include "Ticker.h"
#include <QDebug>
#include <algorithm>

Ticker::Ticker() {
    data[0] = '\0';
}

Ticker::Ticker(const char* symbol) {
    setSymbol(symbol);
}

Ticker::Ticker(const QString& symbol) {
    setSymbol(symbol.toUtf8().constData());
}

Ticker::Ticker(const Ticker& other) {
    std::memcpy(data, other.data, sizeof(data));
}

Ticker& Ticker::operator=(const Ticker& other) {
    if (this != &other) {
        std::memcpy(data, other.data, sizeof(data));
    }
    return *this;
}

Ticker& Ticker::operator=(const QString& symbol) {
    setSymbol(symbol.toUtf8().constData());
    return *this;
}

QString Ticker::toString() const {
    return QString::fromUtf8(data);
}

Ticker::operator QString() const {
    return toString();
}

const char* Ticker::c_str() const {
    return data;
}

bool Ticker::operator==(const Ticker& other) const {
    return std::strcmp(data, other.data) == 0;
}

bool Ticker::operator!=(const Ticker& other) const {
    return !(*this == other);
}

bool Ticker::operator<(const Ticker& other) const {
    return std::strcmp(data, other.data) < 0;
}

bool Ticker::operator<=(const Ticker& other) const {
    return std::strcmp(data, other.data) <= 0;
}

bool Ticker::operator>(const Ticker& other) const {
    return std::strcmp(data, other.data) > 0;
}

bool Ticker::operator>=(const Ticker& other) const {
    return std::strcmp(data, other.data) >= 0;
}

bool Ticker::operator==(const QString& other) const {
    return toString() == other;
}

bool Ticker::operator!=(const QString& other) const {
    return !(*this == other);
}

bool Ticker::isEmpty() const {
    return data[0] == '\0';
}

int Ticker::length() const {
    return static_cast<int>(std::strlen(data));
}

bool Ticker::isValid() const {
    if (isEmpty()) {
        return false;
    }
    
    int len = length();
    if (len > 5) {
        return false;
    }
    
    // Check for valid characters: alphanumeric, dot, and caret
    for (int i = 0; i < len; ++i) {
        char c = data[i];
        if (!((c >= 'A' && c <= 'Z') || 
              (c >= 'a' && c <= 'z') || 
              (c >= '0' && c <= '9') || 
              c == '.' || 
              c == '^')) {
            return false;
        }
    }
    
    return true;
}

void Ticker::setSymbol(const char* symbol) {
    if (!symbol) {
        data[0] = '\0';
        return;
    }
    
    size_t len = std::strlen(symbol);
    if (len > 5) {
        qWarning() << "Ticker symbol too long (max 5 chars):" << symbol << "- truncating";
        len = 5;
    }
    
    std::memcpy(data, symbol, len);
    data[len] = '\0';
}
