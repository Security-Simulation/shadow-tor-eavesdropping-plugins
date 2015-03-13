COREDIR:=core
SOURCE:=report
CORES:=$(wildcard ${COREDIR}/*.tex )
BIBLIO:=${COREDIR}/*.bib

all: ${SOURCE}.pdf

.PHONY: all clean

# Execute pdflatex 3 times, workaround to get working \label and \ref tags.
%.pdf: ${CORES} ${SOURCE}.tex ${BIBLIO}
	pdflatex ${SOURCE}.tex && bibtex ${SOURCE} && pdflatex ${SOURCE}.tex && pdflatex ${SOURCE}.tex && (rm *.log *.out || true)

clean:
	rm *.aux *.log *.out *.pdf core/*.aux core/*.out 2> /dev/null || true
