/[ \t]*#define[ \t]+(EV|SYN|KEY|BTN|REL|ABS|SW|MSC|LED|REP|SND)_[A-Z0-9_]+/ {
	print "	{"
	print "		.name = \""$2 "\","
	print "		.code = " $2 ","
	print "	},"
}
