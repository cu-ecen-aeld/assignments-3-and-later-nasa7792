#! /bin/sh
#use already provided load and unloaded scripts
case "$1" in 
    start)
    echo "starting aesdchar application"
    aesdchar_load
    ;;
    stop)
    echo "stopping aesdchar application"
    aesdchar_unload
    ;;
    *)
    echo "usage: $0 {start|stop}"
    exit 1
    esac
exit 0
