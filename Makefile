# Online Food Ordering System — C++ build
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Iinclude -pthread
LDFLAGS  ?= -pthread
LDLIBS   ?= -lsqlite3

SRC  := $(wildcard src/*.cpp)
OBJ  := $(patsubst src/%.cpp,build/%.o,$(SRC))
BIN  := ofos

.PHONY: all run clean test fresh

all: $(BIN)

$(BIN): $(OBJ)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

build/%.o: src/%.cpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Run the server on http://127.0.0.1:8080
run: $(BIN)
	./$(BIN) --host 127.0.0.1 --port 8080 --dir .

# Start fresh with an empty database (re-seeds sample menu + admin).
fresh:
	rm -f data/ofos.db data/ofos.db-wal data/ofos.db-shm
	$(MAKE) run

# End-to-end black-box test suite (28 cases) against a running build.
test: $(BIN)
	bash tests/run_tests.sh

clean:
	rm -rf build $(BIN)
