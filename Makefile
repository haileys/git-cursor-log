CXXFLAGS = -Wall -Wextra -pedantic -O3
LDFLAGS = -lgit2

.PHONY: all clean

all: git-cursor-log

clean:
	rm -f git-cursor-log
