PROG=mcts
DEBUG_FLAGS=-g -DDEBUG
CXXFLAGS=-std=c++11 $(DEBUG_FLAGS)

.DEFAULT_GOAL := $(PROG)

.PHONY: clean
clean:
	rm -rf $(PROG) $(PROG).o $(PROG).dSYM
