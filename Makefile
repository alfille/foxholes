CC = gcc
NAME = foxhole_solver

CFLAGS = -W -Wall -Wextra -pedantic -O3

OBJS = $(NAME).o

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
  
$(NAME): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

clean:
	rm -f *.o
	rm -f *~
	rm -f $(NAME)
