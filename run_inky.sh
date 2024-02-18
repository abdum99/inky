#!/bin/sh
echo "Starting Inky"
tmux new-session -d -s inky "sudo /home/pi/inky/inky"
tmux ls
echo "Started Tmux Session "
echo "Inky should be running now"
