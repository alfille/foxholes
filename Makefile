CC = gcc
NAMES = foxhole_solver foxhole64_solver

CFLAGS = -W -Wall -Wextra -pedantic -O3

OBJS = $(NAME).o

all: $(NAMES)

$(NAMES): $(NAME=$(.TARGET))

$(NAME): $(OBJS)
	$(CC) $(CFLAGS) -o $(.TARGET) $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.core
	rm -f *~
	rm -f $(NAMES) $(NAMES).o

run:
	./$(NAME)
	
