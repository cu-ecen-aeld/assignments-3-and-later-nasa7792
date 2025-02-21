#! /bin/sh

case "$1" in 
    start)
    echo "starting aesd assignment 5 application"
    start-stop-daemon -S -n aesdsocket -a /usr/bin/aesdsocket -- -d
    ;;
    stop)
    echo "stopping aesd assignment 5 application"
    start-stop-daemon -K -n aesdsocket
    ;;
    *)
    echo "usage: $0 {start|stop}"
    exit 1
    esac
exit 0
