#!/bin/sh
cd "$(dirname "$0")"
./flash.sh
status=$?
printf "\nНажмите Enter для закрытия..."
read -r answer
exit "$status"
