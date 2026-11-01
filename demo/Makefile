all: obedient stubborn orphan runaway leashed sneaky

%: %.c
	gcc -O0 -o $@ $<

sneaky: sneaky.s
	gcc -nostdlib -static -o $@ $<

clean:
	rm -f obedient stubborn orphan runaway leashed sneaky
