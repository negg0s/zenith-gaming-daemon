#!/bin/sh
if [ -d /run/systemd/system ]; then
    systemctl daemon-reload 2>/dev/null || true
    systemctl enable zenith.service 2>/dev/null || true
fi
exit 0
