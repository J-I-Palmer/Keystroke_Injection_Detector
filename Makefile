LIBS = User32.lib

.PHONY: clean all

all: injection_detector

injection_detector: injection_detector.cpp
	cl /EHsc injection_detector.cpp ${LIBS}

clean:
	del /Q *.obj *.exe