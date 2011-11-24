#ifndef _REMOTE_CONTROL_IRKEY__H_
#define _REMOTE_CONTROL_IRKEY__H_ 1


#define HEADER_VERSION_MASK     0x0F
#define HEADER_VERSION_SHIFT    0
#define HEADER_PROTOCOL_MASK    0xF0
#define HEADER_PROTOCOL_SHIFT   4
#define HEADER_PROTOCOL_RC5     0x1
#define HEADER_PROTOCOL_RC6     0x2
#define HEADER_PROTOCOL_LG      0x3

struct ir_message {
	uint8_t header;
	uint8_t reserved;
	union {
		struct {
			uint8_t d5;
			uint8_t d4;
			uint8_t d3;
			uint8_t d2;
			uint8_t d1;
			uint8_t d0;
		};
		uint8_t data[6];
	};
};


struct irkey;

struct irkey* irk_new();
struct irkey* irk_new_with_tty(const gchar *tty);
void irk_free(struct irkey *ctx);

int irk_setup_thread(struct irkey *ctx);
int irk_close_thread(struct irkey *ctx);

int irk_peek_message(struct irkey *ctx, struct ir_message **msg);

#endif /* _REMOTE_CONTROL_IRKEY__H_ */