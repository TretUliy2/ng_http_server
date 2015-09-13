default:
	gcc5 -c main.c
	gcc5 -lc -lnetgraph -o http_server *.o
clean:
	rm -rf *.o *.core http_server
