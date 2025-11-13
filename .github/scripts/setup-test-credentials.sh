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

# Create SecureStorage.ini (pre-obfuscated sensitive tokens)
# Note: The secret values are already obfuscated, so we write them directly
cat > "$CONFIG_DIR/SecureStorage.ini" << EOF
[TradeStation]
access_token=$ACCESS_TOKEN
client_id=$CLIENT_ID
client_secret=$CLIENT_SECRET
id_token=$ID_TOKEN
refresh_token=$REFRESH_TOKEN
EOF

echo "Created SecureStorage.ini"
echo "Auth configuration files created successfully"
echo "Location: $CONFIG_DIR"
ls -la "$CONFIG_DIR"
