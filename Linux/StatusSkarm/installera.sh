#!/bin/bash
# installera.sh — installerar statusskärmen på robotens Pi. Körs PÅ Pi:n, som den
# användare som loggar in i skrivbordet (t.ex. macbot-slu), från den här mappen:
#
#   cd ~/rise_sdvp/Linux/StatusSkarm && bash installera.sh
#
# Gör:
#   1. Rust via rustup om det saknas (i hemkatalogen, ingen sudo)
#   2. Bygger statusskarm och lägger den i ~/.local/bin
#   3. sudoers-regel: EXAKT dessa kommandon utan lösenord för den här användaren,
#      inget annat: omstart av car_client, car_rtk och WireGuard (wg-quick@wg0),
#      omstart av Pi:n och läsning av WireGuards senaste handskakning
#   4. Startar automatiskt i skrivbordet (helskärm), startar om sig själv om den kraschar
#
# Säkert att köra igen efter git pull (bygger om, skriver om regeln och autostarten).
# Ta bort:  bash installera.sh --ta-bort

set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
ANV="$(id -un)"
SUDOERS=/etc/sudoers.d/statusskarm
AUTOSTART="$HOME/.config/autostart/statusskarm.desktop"

if [ "${1:-}" = "--ta-bort" ]; then
  pkill -x statusskarm || true
  rm -f "$AUTOSTART" "$HOME/.local/bin/statusskarm" "$HOME/.local/bin/statusskarm-loop"
  sudo rm -f "$SUDOERS"
  echo "Statusskärmen borttagen."
  exit 0
fi

echo "--- Rust ---"
if ! command -v cargo >/dev/null 2>&1 && [ ! -x "$HOME/.cargo/bin/cargo" ]; then
  curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y --profile minimal
fi
# shellcheck disable=SC1091
source "$HOME/.cargo/env" 2>/dev/null || true

echo "--- Bygger (några minuter första gången) ---"
cargo build --release
mkdir -p "$HOME/.local/bin"
install -m 755 target/release/statusskarm "$HOME/.local/bin/statusskarm"

# Startar om appen om den skulle krascha (men inte i en snabb loop).
cat > "$HOME/.local/bin/statusskarm-loop" <<'LOOP'
#!/bin/bash
while true; do
  "$HOME/.local/bin/statusskarm" "$@"
  sleep 3
done
LOOP
chmod 755 "$HOME/.local/bin/statusskarm-loop"

echo "--- sudoers-regel för $ANV (bara de sex kommandona) ---"
TMP="$(mktemp)"
cat > "$TMP" <<RULE
# statusskärmen (rise_sdvp/Linux/StatusSkarm): exakt dessa kommandon, inget annat.
$ANV ALL=(root) NOPASSWD: /usr/bin/systemctl restart car_client.service
$ANV ALL=(root) NOPASSWD: /usr/bin/systemctl restart car_rtk.service
$ANV ALL=(root) NOPASSWD: /usr/bin/systemctl reboot
$ANV ALL=(root) NOPASSWD: /usr/bin/systemctl poweroff
$ANV ALL=(root) NOPASSWD: /usr/bin/systemctl restart wg-quick@wg0.service
$ANV ALL=(root) NOPASSWD: /usr/bin/wg show wg0 latest-handshakes
RULE
# Kontrollera syntaxen innan den läggs på plats: en trasig sudoers-fil kan låsa ute sudo.
sudo visudo -cf "$TMP"
sudo install -m 440 -o root -g root "$TMP" "$SUDOERS"
rm -f "$TMP"

echo "--- Autostart i skrivbordet ---"
mkdir -p "$(dirname "$AUTOSTART")"
cat > "$AUTOSTART" <<DESK
[Desktop Entry]
Type=Application
Name=Statusskärm
Comment=Robotens status i helskärm (5" touch)
Exec=$HOME/.local/bin/statusskarm-loop
X-GNOME-Autostart-enabled=true
DESK

echo
echo "Klart. Startar automatiskt vid nästa inloggning i skrivbordet."
echo "Starta nu:      $HOME/.local/bin/statusskarm-loop &"
echo "Kolla signaler: statusskarm --text     (utan skärm, över ssh)"
echo "Ta bort:        bash installera.sh --ta-bort"
