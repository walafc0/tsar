
HPP_FILES = $(wildcard *.hpp)


all: generate_test

generate_test: Genere_Tests.cpp $(HPP_FILES) config.h
	g++ -o $@ $<

clean:
	rm -f generate_test
