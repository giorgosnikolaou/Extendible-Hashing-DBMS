# Paths
INCLUDE = ../../include
MODULES = ../../src
TESTS = .
LIB = ../../lib

# Compiler
CC = gcc

# Compile options
CFLAGS = -Wall -g -Werror -I$(INCLUDE)


# .o files needed
OBJS 	= $(MODULES)/hash_file.o $(TESTS)/multiple_files.o
OBJS1	= $(MODULES)/hash_file.o $(TESTS)/insertion_find_time.o
OBJS2	= $(MODULES)/hash_file.o $(TESTS)/ht_main_test.o
OBJS3	= $(MODULES)/hash_file.o $(MODULES)/sht_file.o $(TESTS)/test.o
OBJS4	= $(MODULES)/hash_file.o $(MODULES)/sht_file.o $(TESTS)/essentials.o $(TESTS)/test_join.o

# Executable
EXEC 	= multiple_files
EXEC1 	= insertion_find_time
EXEC2	= ht_main_test
EXEC3	= test
EXEC4	= inner_join_test


$(EXEC): $(OBJS)
	@$(CC) -L $(LIB)/ -Wl,-rpath,$(LIB)/ $(OBJS) -lbf -o $(EXEC)

$(EXEC1): $(OBJS1)
	@$(CC) -L $(LIB)/ -Wl,-rpath,$(LIB)/ $(OBJS1) -lbf -o $(EXEC1)

$(EXEC2): $(OBJS2)
	@$(CC) -L $(LIB)/ -Wl,-rpath,$(LIB)/ $(OBJS2) -lbf -o $(EXEC2)

$(EXEC3): $(OBJS3)
	@$(CC) -L $(LIB)/ -Wl,-rpath,$(LIB)/ $(OBJS3) -lbf -o $(EXEC3)

$(EXEC4): $(OBJS4)
	@$(CC) -L $(LIB)/ -Wl,-rpath,$(LIB)/ $(OBJS4) -lbf -o $(EXEC4)

clean:
	@rm -f $(OBJS) $(EXEC)
	@rm -f $(OBJS1) $(EXEC1)
	@rm -f $(OBJS2) $(EXEC2)
	@rm -f $(OBJS3) $(EXEC3)
	@rm -f $(OBJS4) $(EXEC4)
	@rm -f *.db
	@rm -f *.txt

run: $(EXEC3)
	./$(EXEC3)
	@make -s clean

valgrind: $(EXEC3)
	valgrind --track-origins=yes -s --leak-check=full --show-leak-kinds=all ./$(EXEC3)
	@make -s clean

run1: $(EXEC)
	@./$(EXEC)

# Will send the results to a text file to get the accurate time taken to find the elements
run2: $(EXEC1)
	@./$(EXEC1) > search.txt

run3: $(EXEC2) 
	@./$(EXEC2)

run4: $(EXEC4) 
	@./$(EXEC4)

v4: $(EXEC4)
	valgrind --track-origins=yes -s --leak-check=full --show-leak-kinds=all ./$(EXEC4)
	@make -s clean

tests:
	@echo "Running test 1";
	@make -s run1 
	@make -s clean
	@echo "\n\n\nRunning test 2";
	@make -s run2
	@make -s clean
	@echo "\n\n\nRunning test 3";
	@make -s run
	@make -s clean
	@echo "\n\n\nRunning inner join test";
	@make -s run4
	@make -s clean