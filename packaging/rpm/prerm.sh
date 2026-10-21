#!/bin/sh
if [ "$1" -eq 0 ]; then
    # Full package erasure / uninstall
    if [ -d /run/systemd/system ]; then
        systemctl stop zenith.service 2>/dev/null || true
        systemctl disable zenith.service 2>/dev/null || true
    fi

    # Guaranteed rollback
    if [ -x /usr/bin/zenithctl ]; then
        /usr/bin/zenithctl rollback 2>/dev/null || true
    elif [ -x /usr/bin/zenith ]; then
        /usr/bin/zenith --rollback 2>/dev/null || true
    fi

    rm -rf /run/zenith 2>/dev/null || true
fi
exit 0
