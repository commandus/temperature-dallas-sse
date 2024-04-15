#!/bin/sh
# Create PID file
touch /var/run/temperature-dallas-sse.pid
# Copy systemd service file
cp temperature-dallas-sse.service /etc/systemd/system/
exit 0
