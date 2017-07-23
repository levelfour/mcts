PROG=mcts
CXXFLAGS=-g -std=c++11

.DEFAULT_GOAL := $(PROG)

.PHONY: clean
clean:
	rm -rf $(PROG) $(PROG).o $(PROG).dSYM
