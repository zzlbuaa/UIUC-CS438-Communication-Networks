CXX = g++
LD = g++
CXXFLAGS := -std=c++1z -c -g -O0 -Wall -Wextra -pedantic 
LDFLAGS := -std=c++1z 

all : reliable_sender reliable_receiver

.PHONY: clean all

reliable_sender : sender_main.o
	$(LD) sender_main.o $(LDFLAGS) -o reliable_sender

reliable_receiver : receiver_main.o 
	$(LD) receiver_main.o $(LDFLAGS) -o reliable_receiver

%.o : %.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	@rm -f *.o reliable_sender reliable_receiver