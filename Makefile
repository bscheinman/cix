all:
	$(MAKE) -C shared all
	$(MAKE) -C server all

clean:
	$(MAKE) -C shared clean
	$(MAKE) -C server clean
