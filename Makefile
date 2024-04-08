CXX     = c++
CFLAGS  = -g -O3 -Wall
LIBS    = 
LDFLAGS = -g

OBJECTS = MMDVM-TNC-Tester.o StopWatch.o Thread.o UARTController.o

all:		mmdvm_tnc_tester

mmdvm_tnc_tester:	$(OBJECTS) 
		$(CXX) $(OBJECTS) $(LDFLAGS) $(LIBS) -o mmdvm_tnc_tester

%.o: %.cpp
		$(CXX) $(CFLAGS) -c -o $@ $<

install: all
		install -m 755 mmdvm_tnc_tester /usr/local/bin/

clean:
		$(RM) mmdvm_tnc_tester *.o *.d *.bak *~

