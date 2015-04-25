all:
	$(MAKE) -C shared all
	$(MAKE) -C client all
	$(MAKE) -C server all

clean:
	$(MAKE) -C shared clean
	$(MAKE) -C client clean
	$(MAKE) -C server clean
