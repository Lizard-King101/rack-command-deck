#!/usr/bin/env bash
set -euo pipefail

if [[ $EUID -ne 0 ]]; then echo "Run as root."; exit 1; fi

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
DECK_USER=${COMMAND_DECK_USER:-$(stat -c '%U' "$SCRIPT_DIR/../..")}

install -D -o root -g root -m 0755 "$SCRIPT_DIR/command-deck-update" \
    /usr/local/libexec/command-deck-update
cat > /etc/sudoers.d/command-deck-update <<EOF
$DECK_USER ALL=(root) NOPASSWD: /usr/local/libexec/command-deck-update *
EOF
chmod 0440 /etc/sudoers.d/command-deck-update
visudo -cf /etc/sudoers.d/command-deck-update

echo "Installed dashboard updater for service user: $DECK_USER"
