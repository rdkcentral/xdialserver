DIRS:= server

all:
	for dir in $(DIRS); do (cd $$dir && cmake . && make || exit 1) || exit 1; done
clean:
	for dir in $(DIRS); do (cd $$dir && make clean || exit 1) || exit 1; done
