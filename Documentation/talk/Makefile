COREDIR:=core
SOURCE:=slides
CORE:=$(wildcard $(COREDIR)/*)

all: ${SOURCE}.pdf one 

.PHONY: all one clean

# Execute pdflatex 3 times, workaround to get working \label and \ref tags.
%.pdf: ${SOURCE}.tex $(CORE)
	pdflatex ${SOURCE}.tex && pdflatex ${SOURCE}.tex

one: $(CORE)
	pdflatex ${SOURCE}.tex && (rm *.log *.out || true)

clean:
	rm *.aux *.log *.out *.pdf *.nav *.toc *.snm 2> /dev/null || true
