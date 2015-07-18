COREDIR:=core
SOURCE:=slides
CORE:=$(wildcard $(COREDIR)/*)

all: ${SOURCE}.pdf

.PHONY: all clean

# Execute pdflatex 3 times, workaround to get working \label and \ref tags.
%.pdf: ${SOURCE}.tex $(CORE)
	pdflatex ${SOURCE}.tex && pdflatex ${SOURCE}.tex && pdflatex ${SOURCE}.tex && (rm *.log *.out || true)

clean:
	rm *.aux *.log *.out *.pdf *.nav *.toc *.snm 2> /dev/null || true
