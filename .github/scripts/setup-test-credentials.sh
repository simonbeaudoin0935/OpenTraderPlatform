#!/bin/bash
set -e

# Script to create auth configuration files for unit testing
# This script creates the necessary INI files with expired tokens
# that will be refreshed during the test suite execution

# Check if all required environment variables are set
if [ -z "$ACCESS_TOKEN" ] || [ -z "$CLIENT_ID" ] || [ -z "$CLIENT_SECRET" ] || \
   [ -z "$ID_TOKEN" ] || [ -z "$REFRESH_TOKEN" ] || [ -z "$EXPIRES_IN" ] || \
   [ -z "$RECEIVED_AT" ] || [ -z "$SCOPE" ] || [ -z "$TOKEN_TYPE" ]; then
    echo "Error: Missing required environment variables"
    echo "Required: ACCESS_TOKEN, CLIENT_ID, CLIENT_SECRET, ID_TOKEN, REFRESH_TOKEN, EXPIRES_IN, RECEIVED_AT, SCOPE, TOKEN_TYPE"
    exit 1
fi

# Function to obfuscate values (matching SecureStorage::obfuscateValue)
obfuscate_value() {
    local value="$1"
    # Use Python to perform the same XOR obfuscation as the C++ code
    python3 << EOF
import hashlib
import base64

value = "$value"
key_hash = hashlib.sha256(b"L2TraderSecureStorage").digest()
data = value.encode('utf-8')
obfuscated = bytearray()

for i in range(len(data)):
    obfuscated.append(data[i] ^ key_hash[i % len(key_hash)])

print(base64.b64encode(bytes(obfuscated)).decode('utf-8'))
EOF
}

# Determine config directory
CONFIG_DIR="${HOME}/.config/L2Trader"
mkdir -p "$CONFIG_DIR"

echo "Creating auth configuration files in $CONFIG_DIR"

# Create TradeStationTokens.ini (non-sensitive metadata)
cat > "$CONFIG_DIR/TradeStationTokens.ini" << EOF
[Tokens]
expires_in=$EXPIRES_IN
received_at=$RECEIVED_AT
scope=$SCOPE
token_type=$TOKEN_TYPE
EOF

echo "Created TradeStationTokens.ini"

# Create SecureStorage.ini (obfuscated sensitive tokens)
# The format is: TradeStation/key=obfuscated_value

ACCESS_TOKEN_OBF=$(obfuscate_value "$ACCESS_TOKEN")
REFRESH_TOKEN_OBF=$(obfuscate_value "$REFRESH_TOKEN")
ID_TOKEN_OBF=$(obfuscate_value "$ID_TOKEN")
CLIENT_ID_OBF=$(obfuscate_value "$CLIENT_ID")
CLIENT_SECRET_OBF=$(obfuscate_value "$CLIENT_SECRET")

cat > "$CONFIG_DIR/SecureStorage.ini" << EOF
[TradeStation]
access_token=$ACCESS_TOKEN_OBF
client_id=$CLIENT_ID_OBF
client_secret=$CLIENT_SECRET_OBF
id_token=$ID_TOKEN_OBF
refresh_token=$REFRESH_TOKEN_OBF
EOF

echo "Created SecureStorage.ini"
echo "Auth configuration files created successfully"
echo "Location: $CONFIG_DIR"
ls -la "$CONFIG_DIR"
