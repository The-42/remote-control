/[ \t]*#define[ \t]+(EV|SYN|KEY|BTN|REL|ABS|SW|MSC|LED|REP|SND)_[A-Z0-9_]+/ {
    print "	{"
    print "		.name = \"" $2 "\","
    print "		.getProperty = input_get_event_code,"
    print "		.attributes = kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete,"
    print "	},"
}
