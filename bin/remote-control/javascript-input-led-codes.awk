/[ \t]*#define[ \t]+LED_(MAX|CNT)[ \t]/ {
	next
}

/[ \t]*#define[ \t]+LED_[A-Z0-9_]+/ {
	print "	{"
	print "		.name = \"" substr($2,5) "\","
	print "		.code = " $2 ","
	print "	},"
}
