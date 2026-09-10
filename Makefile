CXX ?= g++
CXXFLAGS ?= -std=c++20 -O3 -Wall -Wextra -pedantic
INCLUDES := -I.

AI_SOURCES := \
	ai/simulation/board.cpp \
	ai/simulation/simulator.cpp \
	ai/evaluation/features.cpp \
	ai/evaluation/weights.cpp \
	ai/evaluation/evaluation.cpp \
	ai/evaluation/trigger_route.cpp \
	ai/evaluation/long_chain_potential.cpp \
	ai/evaluation/forms.cpp \
	ai/search/move_generator.cpp \
	ai/search/beam_search.cpp \
	ai/gtr/gtr_ai.cpp \
	ai/ai.cpp

BENCHMARK_SOURCES := $(AI_SOURCES) ai/benchmark/chain_benchmark.cpp

.PHONY: test benchmark clean

test:
	$(CXX) $(CXXFLAGS) $(INCLUDES) \
		tests/test_native.cpp $(AI_SOURCES) \
		-o /tmp/puyoai_test
	/tmp/puyoai_test

benchmark:
	$(CXX) $(CXXFLAGS) $(INCLUDES) \
		tools/chain_benchmark_cli.cpp $(BENCHMARK_SOURCES) \
		-o /tmp/puyoai_benchmark
	/tmp/puyoai_benchmark 4 60 20260908 2 4

clean:
	rm -f /tmp/puyoai_test /tmp/puyoai_benchmark
