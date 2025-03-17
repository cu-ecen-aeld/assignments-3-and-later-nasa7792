#! /bin/sh
#use already provided load and unloaded scripts
case "$1" in 
    start)
    echo "starting aesdchar application v2.3"
    aesdchar_load
    ;;
    stop)
    echo "stopping aesdchar application v2.3"
    aesdchar_unload
    ;;
    *)
    echo "usage: $0 {start|stop}"
    exit 1
    esac
exit 0
