CXX=g++
USERID=604743589
CLASSES=
#VERSION=-std=c++11
FLAG=-O0 -g

all: clean server client

server_smoke: clean server
	./server 5000

server: $(CLASSES)
	$(CXX) $(VERSION) $(FLAG) -o $@ $^ server.cpp TCPheader.cpp

client: $(CLASSES)
	$(CXX) $(VERSION) $(FLAG) -o $@ $^ client.cpp TCPheader.cpp

clean:
	rm -rf *.o *.data *~ *.swp server client *.tar.gz

dist: tarball
tarball: clean
	tar -cvzf /tmp/$(USERID).tar.gz --exclude=./.vagrant . && mv /tmp/$(USERID).tar.gz .
