default:
	cc -c main.c
	cc -lc -lnetgraph -o http_server *.o
clean:
	rm -rf *.o *.core http_server
