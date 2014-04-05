
#define IRQ_EVENT_PROBE(irq) do { \
				if(irq_debug) \
					if(irq == 57 || irq == 58) \
						kernel_event_monitor(IRQ_EVENT); \
			} while(0)

typedef enum
{	
	NO_EVENT,
	IRQ_EVENT,
} moca_km_enum_type;

extern bool irq_debug;
extern void kernel_event_monitor(moca_km_enum_type type);

