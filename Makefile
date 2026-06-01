CXX      = g++
CURL_INC := $(shell curl-config --cflags 2>/dev/null || echo "-I/usr/include")
CURL_LIB := $(shell curl-config --libs   2>/dev/null || echo "-lcurl")
CXXFLAGS  = -std=c++17 -O2 -Wall -Wextra -Isrc $(CURL_INC)
LDFLAGS   = $(CURL_LIB) -lpthread

TARGET = telemetry
SRCS   = src/main.cpp     \
         src/probe.cpp    \
         src/stats.cpp    \
         src/auditor.cpp  \
         src/reporter.cpp

OBJS = $(SRCS:.cpp=.o)

.PHONY: all clean article check_deps

all: results $(TARGET)

results:
	@mkdir -p results

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Dépendances d'en-têtes (mise à jour automatique)
src/main.o:     src/validator.hpp src/auditor.hpp src/reporter.hpp
src/probe.o:    src/probe.hpp
src/stats.o:    src/stats.hpp
src/auditor.o:  src/auditor.hpp src/probe.hpp src/stats.hpp
src/reporter.o: src/reporter.hpp src/auditor.hpp

clean:
	rm -f $(OBJS) $(TARGET)

article:
	pdflatex -interaction=nonstopmode article_localhost.tex
	pdflatex -interaction=nonstopmode article_localhost.tex

check_deps:
	@pkg-config --exists libcurl \
	  && echo "[OK] libcurl trouvée" \
	  || echo "[ERREUR] sudo apt install libcurl4-openssl-dev"
