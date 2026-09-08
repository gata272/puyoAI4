CXX ?= g++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -pedantic
INCLUDES := -I.

AI_SOURCES := \
	ai/simulation/board.cpp \
	ai/simulation/simulator.cpp \
	ai/evaluation/features.cpp \
	ai/evaluation/weights.cpp \
	ai/evaluation/evaluation.cpp \
	ai/evaluation/forms.cpp \
	ai/search/move_generator.cpp \
	ai/search/beam_search.cpp \
	ai/gtr/gtr_ai.cpp \
	ai/ai.cpp

.PHONY: test benchmark tuner clean

test:
	$(CXX) $(CXXFLAGS) $(INCLUDES) \
		tests/test_native.cpp $(AI_SOURCES) \
		-o /tmp/puyoai3_test
	/tmp/puyoai3_test

benchmark:
	$(CXX) $(CXXFLAGS) $(INCLUDES) \
		tools/benchmark.cpp $(AI_SOURCES) -o /tmp/puyoai3_benchmark
	/tmp/puyoai3_benchmark 4 40

TUNER_ITER ?= 20
TUNER_GAMES ?= 6
TUNER_TURNS ?= 35

tuner:
	$(CXX) $(CXXFLAGS) $(INCLUDES) \
		tools/tuner.cpp $(AI_SOURCES) -o /tmp/puyoai3_tuner
	/tmp/puyoai3_tuner $(TUNER_ITER) $(TUNER_GAMES) $(TUNER_TURNS)

clean:
	rm -f /tmp/puyoai3_test
	rm -f ai/**/*.o
