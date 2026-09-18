#!/bin/bash
# Ge USB-portarna, modemet och VPN-tunneln 10 sekunder att vakna efter boot
sleep 10

# Startar Car_Client i en bakgrunds-screen
screen -S car -d -m bash -c "cd '/home/robant/RControllStation/rise_sdvp/Linux/Car_Client' && ./Car_Client -p /dev/vehicle --useudp --logusb --usetcp --tcprtcmserver 8200 --tcpubxserver 8210 --setid 4; bash"
echo "Car_Client startades i en screen-session med namnet 'car'."
echo "För att ansluta live, kör: screen -r car"
