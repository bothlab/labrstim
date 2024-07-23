#!/bin/sh

# we want to make an UTF-8 locale available and default

sed -i 's/^# *\(en_IE.UTF-8\)/\1/' /etc/locale.gen
locale-gen
update-locale LANG=en_IE.UTF-8
