all:
	$(MAKE) -C shared all
	$(MAKE) -C server all
	$(MAKE) -C client all

clean:
	$(MAKE) -C shared clean
	$(MAKE) -C server clean
	$(MAKE) -C client clean
