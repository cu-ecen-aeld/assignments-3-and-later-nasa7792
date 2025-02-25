#use flags for wall and werror all warnings will be treated as errors
all : aesdsocket
aesdsocket : aesdsocket.c
	$(CC) -Wall -Werror -o aesdsocket aesdsocket.c
.PHONY : clean
clean:
	rm -f aesdsocket