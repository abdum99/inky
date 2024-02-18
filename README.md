inky uses MQTT to communicate with Osiris
You need to install libmosquitto-dev which is available on apt-get:

```
sudo apt-get update
sudo apt-get install libmosquitto-dev
```
a systemd .service config file is provided under `./systemd/inky.service`
copy it to `/lib/systemd/system/inky.service`
then start `sudo systemctl start inky.service`
> (You may need to reload the daemon with `sudo systemctl daemon-reload`
