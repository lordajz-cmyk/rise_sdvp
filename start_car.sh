#!/bin/bash
# Startar Car_Client i en bakgrunds-screen med en initial fördröjning inuti screen (icke-blockerande för systemd)
screen -S car -d -m bash -c "sleep 15 && cd '/home/robant/RControllStation/rise_sdvp/Linux/Car_Client' && ./Car_Client -p /dev/vehicle --useudp --logusb --usetcp --tcprtcmserver 8200 --tcpubxserver 8210 --setid 4; bash"
echo "Car_Client startades i en screen-session med namnet 'car'."
echo "För att ansluta live, kör: screen -r car"
