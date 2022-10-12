CC = gcc
NAMES = foxhole32_solver foxhole64_solver fhsolve
BINDIR = /usr/bin

CFLAGS = -W -Wall -Wextra -pedantic -O3

OBJS = $(NAME).o

all: $(NAMES)

$(NAMES): $(NAME=$(.TARGET))

$(NAME): $(OBJS)
	$(CC) $(CFLAGS) -o $(.TARGET) $(OBJS)
	chmod +x $(NAME)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

install:
	install -m 557 $(NAMES) $(BINDIR)

clean:
	rm -f *.core
	rm -f *~
	rm -f $(NAMES) $(NAMES).o

run:
	./$(NAME)
	
