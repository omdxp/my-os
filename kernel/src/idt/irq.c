#include "idt/irq.h"
#include "io/io.h"

void irq_disable(IRQ irq)
{
	int port = IRQ_MASTER_PORT;
	IRQ relative_irq = irq;

	if (irq >= PIC_SLAVE_STARTING_IRQ)
	{
		port = IRQ_SLAVE_PORT;
		relative_irq -= PIC_SLAVE_STARTING_IRQ;
	}

	// read the current value of the PIC's mask register
	int pic_value = insb(port);
	pic_value |= (1 << relative_irq); // set the bit corresponding to the IRQ to 1 (disable)
	outb(port, pic_value);			  // write the new value back to the PIC's mask
}

void irq_enable(IRQ irq)
{
	int port = IRQ_MASTER_PORT;
	IRQ relative_irq = irq;

	if (irq >= PIC_SLAVE_STARTING_IRQ)
	{
		port = IRQ_SLAVE_PORT;
		relative_irq -= PIC_SLAVE_STARTING_IRQ;
	}

	// read the current value of the PIC's mask register
	int pic_value = insb(port);
	pic_value &= ~(1 << relative_irq); // set the bit corresponding to the IRQ to 0 (enable)
	outb(port, pic_value);			   // write the new value back to the PIC's mask
}
